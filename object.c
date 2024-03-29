#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

/// @brief 指定したサイズのオブジェクトをヒープに割り当てる
/// @param size バイト数
/// @param type 
/// @return 
static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = vm.objects;
    vm.objects = object;

    #ifdef DEBUG_LOG_GC
    // メモリ割り当てのログ
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
    #endif
    
    return object;
}

ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* new_class(ObjString* name) {
    ObjClass* class_ = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    class_->name = name;
    init_table(&class_->methods);
    return class_;
}

ObjClosure* new_closure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjFunction* new_function() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

ObjInstance* new_instance(ObjClass* class_) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->class_ = class_;
    init_table(&instance->fields);
    return instance;
}

ObjNative* new_native(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

/// @brief 文字列オブジェクトを作る
/// @param chars 
/// @param length 
/// @return 
static ObjString* allocate_string(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string)); // GC対策
    table_set(&vm.strings, string, NIL_VAL);
    pop();
    return string;
}

/// @brief 文字列のハッシュを計算する（FNV-1a）
/// @param key 
/// @param length 
/// @return 
static uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* take_string(char* chars, int length) {
    uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        // 既に文字列が存在すれば，渡されたものは開放する
        // この関数に所有権が譲渡されているから
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

ObjString* copy_string(const char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    // 文字列の複製の有無を確認し，既にあればそれを返す
    if (interned != NULL) {
        return interned;
    }

    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length, hash);
}

ObjUpvalue* new_upvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

/// @brief 関数をプリントする
/// @param function 
static void print_function(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    
    printf("<fn %s>", function->name->chars);
}

void print_object(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            print_function(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->class_->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}