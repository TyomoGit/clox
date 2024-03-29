#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

/// @brief 唯一の仮想マシン
Vm vm;

static Value clock_native(int arg_count, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/// @brief vm.stackをリセットする
static void reset_stack() {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

/// @brief ランタイムエラーを発出する
/// @param format エラーの書式
/// @param ... エラーの書式の値
static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack();
}

/// @brief ネイティブ関数を定義する
/// @param name 定義する名前
/// @param function ネイティブ関数
static void define_native(const char* name, NativeFn function) {
    // GCに消されないように一旦pushしてpopする
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_native(function)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void init_vm() {
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_gc = 1024 * 1024;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    init_table(&vm.globals);
    init_table(&vm.strings);

    vm.init_string = NULL;
    vm.init_string = copy_string("init", 4);

    // ネイティブ関数の定義
    define_native("clock", clock_native);
}

void free_vm() {
    free_table(&vm.globals);
    free_table(&vm.strings);
    vm.init_string = NULL;
    free_objects();
}

void push(Value value) {
    *vm.stack_top = value;
    vm.stack_top += 1;
}

Value pop() {
    vm.stack_top -= 1;
    return *vm.stack_top;
}

/// @brief スタックからポップせずにValueを返す
/// @param distance スタックの一番上からどれだけ離れているか（0なら一番上）
/// @return スタックの値
static Value peek(int distance) {
    return vm.stack_top[-1 - distance];
}

/// @brief 関数を呼び出す
/// @param function 呼び出される関数
/// @param arg_count 引数の個数
/// @return エラーがなければtrue
static bool call(ObjClosure* closure, int arg_count) {
    if (arg_count != closure->function->arity) {
        runtime_error("Expected %d arguments but got %d.", closure->function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count];
    vm.frame_count += 1;

    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

/// @brief コールを実行する
/// @param callee 実行対象
/// @param arg_count 引数の個数
/// @return エラーがなければtrue
static bool call_value(Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound->receiver;

                return call(bound->method, arg_count);
            }
            case OBJ_CLASS: {
                ObjClass* class_ = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(new_instance(class_));

                Value initializer;
                if (table_get(&class_->methods, vm.init_string, &initializer)) {
                    return call(AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    // init()が存在しないとき
                    runtime_error("Expected 0 arguments but got %d.", arg_count);
                    return false;
                }

                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }
    runtime_error("Can only call functions and classes.");
    return false;
}

/// @brief クラスからメソッドの参照と呼び出しを行う
/// @param class_ メソッドが属するクラス
/// @param name メソッド名
/// @param arg_count 引数の個数
/// @return 成功したかどうか
static bool invoke_from_class(
    ObjClass* class_,
    ObjString* name,
    int arg_count
) {
    Value method;
    if (!table_get(&class_->methods, name, &method)) {
        runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    return call(AS_CLOSURE(method), arg_count);
}

/// @brief メソッドの参照と呼び出しを行う
/// @param name メソッド名
/// @param arg_count 引数の個数
/// @return 成功したかどうか
static bool invoke(ObjString* name, int arg_count) {
    Value receiver = peek(arg_count);

    if (!IS_INSTANCE(receiver)) {
        runtime_error("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (table_get(&instance->fields, name, &value)) {
        // フィールドを先に探す
        vm.stack_top[-arg_count - 1] = value;
        return call_value(value, arg_count);
    }
    
    // クラスを後に探す
    return invoke_from_class(instance->class_, name, arg_count);
}

/// @brief メソッドをインスタンスに束縛する
/// @param class_ クラス
/// @param name メソッド名
/// @return メソッドが存在するかどうか
static bool bind_method(ObjClass* class_, ObjString* name) {
    Value method;
    if (!table_get(&class_->methods, name, &method)) {
        runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = new_bound_method(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));

    return true;
}

/// @brief ローカル変数をキャプチャする
/// @param local キャプチャされるローカル変数
/// @return 上位値オブジェクト
static ObjUpvalue* capture_upvalue(Value* local) {
    ObjUpvalue* prev_upvalue = NULL;
    ObjUpvalue* upvalue = vm.open_upvalues;

    // スロットが等しい -> 既にこの変数をキャプチャしている上位値があるので，これを使う（ある）
    // リストを最後まで見終わった -> 見つけられなかった（ない）
    // 下のローカルスロットを持つ上位値を見つけた -> 探している上位値の位置を既に通過した（ない）
    while (upvalue != NULL && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* created_upvalue = new_upvalue(local);
    created_upvalue->next = upvalue;

    if (prev_upvalue == NULL) {
        // リストが空のときは，先頭に挿入する
        vm.open_upvalues = created_upvalue;
    } else {
        // upvalueよりも前の位置に挿入
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

/// @brief 上位値をクローズしてヒープに移す
/// @param last ここで指定されるスロットか，ここより上にある上位値を探して，クローズする
static void close_upvalues(Value* last) {
    while (
        vm.open_upvalues != NULL
        && vm.open_upvalues->location >= last
    ) {
        ObjUpvalue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

/// @brief メソッドを定義する
/// @param name メソッドの名前
static void define_method(ObjString* name) {
    Value method = peek(0);
    ObjClass* class_ = AS_CLASS(peek(1));
    table_set(&class_->methods, name, method);
    pop();
}

/// @brief 値が偽性かどうかを判定する
/// @param value 判定される値
/// @return 値が偽性かどうか
static bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/// @brief 文字列を連結する
static void concatenate() {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

/// @brief 仮想マシンを実行する
/// @return 結果
static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

    // 命令を読み込む
    #define READ_BYTE() (*frame->ip++)
    // 定数を読み込む
    #define READ_CONSTANT() \
        (frame->closure->function->chunk.constants.values[READ_BYTE()])
    // チャンクから2バイトを読み出して，16ビットの符号なし整数を取り出す
    #define READ_SHORT() \
        (frame->ip += 2, \
        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    // 文字列を定数部から読み込む
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    // do-whileなのは，ブロックを使うかつセミコロンを後ろに置けるようにするため
    #define BINARY_OP(value_type, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtime_error("Operands must be a numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(value_type(a op b)); \
        } while (false)

    #ifdef DEBUG_TRACE_EXECUTION
    printf("== stack at runtime ==\n");
    #endif

    for(;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
                printf("[ ");
                print_value(*slot);
                printf(" ]");
            }
            printf("\n");

            disassemble_instruction(
                &frame->closure->function->chunk,
                (int)(frame->ip - frame->closure->function->chunk.code)
            );
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT:
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break; 
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_POP:
                pop();
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                // 代入は式なのでpopしない
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable \'%s\'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable \'%s\'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtime_error("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(0));
                ObjString* name = READ_STRING();

                // フィールドを先に探す
                Value value;
                if (table_get(&instance->fields, name, &value)) {
                    pop();
                    push(value);
                    break;
                }

                // メソッドを後に探す
                if (!bind_method(instance->class_, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtime_error("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(1));
                table_set(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop());
                if (!bind_method(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be two number or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
                break;
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT: {
                print_value(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int arg_count = READ_BYTE();
                if (!invoke(method, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int arg_count = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop());
                if (!invoke_from_class(superclass, method, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = new_closure(function);
                push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(frame->slots + index);
                    } else {
                        // 外側の関数から上位値を取り出す
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm.stack_top - 1);
                pop();
                break;
            case OP_RETURN: {
                Value result = pop();
                close_upvalues(frame->slots);
                vm.frame_count -= 1;
                if (vm.frame_count <= 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(new_class(READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtime_error("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* subclass = AS_CLASS(peek(0));
                // 表をコピー
                table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop(); // サブクラス
                break;
            }
            case OP_METHOD:
                define_method(READ_STRING());
                break;
        }
    }

    #undef READ_BYTE
    #undef READ_SHORTl
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);

    // エラーがあれば実行せずに終了
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    // GCに消されないように一旦プッシュ
    push(OBJ_VAL(function));
    ObjClosure* closure = new_closure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}