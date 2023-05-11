#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "assert.h"
#define HASH_MAX 109
#define FRAME_DEPTH 256
#define VECTOR_MAX 12
#define NUM_RESERVED 19
#define NUM_INSIDE 12

#define next_char(lex)  lex->curr_char = lex->content[++lex->pos]
#define look_char(lex)  lex->content[lex->pos + 1]

#define vector_struct(T,vector)  \
typedef struct  {                \
	T *data;                     \
	int n;                       \
	int capacity;                \
} vector

#define vector_push(this, T, value)                                  \
do{                                                                  \
    if ((this)->n + 1 > (this)->capacity) {                          \
	int capacity = ((this)->capacity+1)* 2;                          \
	(this)->data = (T *) realloc((this)->data, capacity * sizeof(T));\
	if ((this)->data == NULL) {                                      \
		printf("memory expansion failed");                           \
		exit(1);                                                     \
	}                                                                \
	(this)->capacity = capacity;                                     \
    }                                                                \
    ((this)->data)[(this)->n] = value;                               \
    (this)->n++;                                                     \
} while (0)

#define vector_pop(this)    \
({                          \
	assert((this)->n > 0);  \
	(this)->n--;            \
	(this)->data[(this)->n];\
})

#define vector_init(this, T, size)                            \
do{                                                           \
	(this)->n = 0;                                            \
	(this)->capacity = size;                                  \
	(this)->data = (T *) malloc((this)->capacity * sizeof(T));\
}while (0)

#define vector_last(this)           \
    ({                              \
        assert(!((this)->n == 0));  \
        (this)->data[(this)->n - 1];\
    })

#define save_char(this,v) vector_push(&this->buff,char,v)
#define save_value(this,v) vector_push(&this->value,Value,v)
#define save_runtime_stack(this,v) vector_push(&this->runtime,Value,v)
#define save_field(this,v) vector_push(&this->fields,Field,v)
#define vector_new(this, T) vector_init(this, T, VECTOR_MAX)
#define is_func_obj(v)  v.type == OBJECT && v.u.object->in_class == &G_function_class
#define is_map_string(a,b)  \
a.type == OBJECT && a.u.object->in_class == &G_map_class && b.type == OBJECT && b.u.object->in_class == &G_string_class
#define is_list_integer(a,b)  a.type == OBJECT && a.u.object->in_class == &G_list_class && b.type == INTEGER

#define OBJECT_HEAD                 \
	struct ClassObject_ * in_class ;\
	int mark;                       \
	struct MyObject_ * next;

#define PARSER_HEAD           \
	OBJECT_HEAD               \
	struct ModuleObject_ *top;\
	VectorInstr in_code;      \
	VectorValue value;        \
	VectorInt var;            \
	StringPos name;

typedef int StringPos;
typedef int Var;

const char *reserve[] = { "var", "function", "if", "else", "true", "false", "while", "for", "break", "return", "null",
		"class", "this", "of", "extends", "super", "new", "import", "continue" };

typedef enum ValueKind_ {
	INTEGER, NUMBER, OBJECT, TRUE, FALSE, NONE, UNDEFINED
} ValueKind;

typedef enum TokenKind_ {
	TOKEN_VAR = 257,/*global*/
	TOKEN_FUNCTION,/*function*/
	TOKEN_IF,/*if*/
	TOKEN_ELSE,/*else*/
	TOKEN_TRUE,/*true*/
	TOKEN_FALSE,/*false*/
	TOKEN_WHILE,/*while*/
	TOKEN_FOR,/*for*/
	TOKEN_BREAK,/*break*/
	TOKEN_RETURN,/*return*/
	TOKEN_NULL,/*NULL*/
	TOKEN_CLASS,/*class*/
	TOKEN_THIS,/*this*/
	TOKEN_OF,/*of*/
	TOKEN_EXTENDS,/*extends*/
	TOKEN_SUPER,/*super*/
	TOKEN_NEW,/*new*/
	IMPORT_TOKEN,/*import*/
	TOKEN_CONTINUE,/*continue*/
	TOKEN_NUMBER,/*16.23*/
	TOKEN_INTEGER,/*16.23*/
	TOKEN_STRING,/*"helloworld"*/
	STRING_FOEMAT_TOKEN,/*"hello%sworld"*/
	TOKEN_OR,/*||*/
	TOKEN_AND,/*&&*/
	TOKEN_LT_EQ,/*<=*/
	TOKEN_GT_EQ,/*>=*/
	TOKEN_EQ,/*==*/
	TOKEN_NOT_EQ,/*!=*/
	TOKEN_NEWLINE,/*\n*/
	TOKEN_IDENTIFIER,/*var*/
	TOKEN_EOF/*end*/
} TokenKind;

typedef enum OpKind_ {
	OP_LOAD_GLOBAL,
	OP_BUILD_OBJECT,
	OP_BUILD_LIST,
	OP_BUILD_CLASS,
	OP_BUILD_MAP,
	OP_FUNC_CALL,
	OP_ARRAY_LENGTH,
	OP_STORE_ATTR,
	OP_LOAD_ATTR,
	OP_RETURN,
	OP_LOAD,
	OP_INDEX,
	OP_THIS,
	OP_SUPER,
	OP_CONST_INT,
	OP_CONST_NULL,
	OP_CONST_FALSE,
	OP_CONST_TRUE,
	OP_PUSH,
	OP_STORE,
	OP_STORE_INDEX,
	OP_GOTO,
	OP_GOTO_IF_FALSE,
	OP_NOT,
	OP_MINUS,
	OP_NEW,
	OP_PLUS,
	OP_PLUS_SELF,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_EQ,
	OP_NOT_EQ,
	OP_OR,
	OP_AND,
	OP_LT_EQ,
	OP_GT_EQ,
	OP_NE,
	OP_GT,
	OP_GE,
	OP_LT,
	OP_LE,
	OP_END
} OpKind;

struct Instruction_;
struct StringObject_;
struct ClassObject_;
struct InstanceObject_;
struct MyParserObject_;
struct Interpreter_;
struct GlobalInfo_;
struct Frame_;
struct ModuleObject_;
struct FunctionObject_;
struct ClassCodeObject_;
struct Lexer_;
struct Value_;
struct Field_;

vector_struct(int, VectorInt);
vector_struct(char, VectorChar);
vector_struct(struct Instruction_, VectorInstr);
vector_struct(struct StringObject_ *, VectorString);
vector_struct(struct Value_, VectorValue);
vector_struct(struct Field_, VectorField);

typedef struct MyObject_ {
	OBJECT_HEAD
} MyObject;

typedef struct Value_ {
	ValueKind type;
	union {
		double number;
		int integer;
		MyObject *object;
	} u;
} Value;

typedef struct Frame_ {
	Value *value;
	int n;
	VectorValue runtime;
} Frame;

typedef struct Field_ {
	Var var;
	Value value;
} Field;

typedef struct GlobalInfo_ {
	struct Lexer_ *lexer;
	struct MyParserObject_ *top_parser;
	Frame frames[FRAME_DEPTH];
	int top_frame;
	MyObject *gc_head;
	int gc_capacity;
	int gc_count;
} GlobalInfo;

typedef struct StringObject_ {
	OBJECT_HEAD
	char *str_char;
	int len;
	int is_const;
} StringObject;

typedef struct ClassObject_ {
	OBJECT_HEAD
	VectorField fields;
	StringPos name;
	void (*mark_obj)(MyObject*);
	void (*free_obj)(MyObject*);
	struct ClassCodeObject_ *code;
	struct ClassObject_ *super;
} ClassObject;

typedef struct ListObject_ {
	OBJECT_HEAD
	VectorValue list;
} ListObject;

typedef struct HashNode_ {
	StringObject *string;
	int hash_value;
	Value v;
	struct HashNode_ *next;
} HashNode;

typedef struct MapObject_ {
	OBJECT_HEAD
	int capacity;
	HashNode **hash_table;
} MapObject;

typedef struct Lexer_ {
	int type;
	int curr_line;
	VectorChar buff;
	char *content;
	int pos;
	char curr_char;
	union {
		int integer;
		double number;
		StringPos str_pos;
	} u;
} Lexer;

typedef struct ExprTempType_ {
	int flag;
	StringPos str_pos;
} ExprTempType;

typedef struct Instruction_ {
	OpKind op;
	int v;
} Instruction;

typedef struct MyParserObject_ {
	PARSER_HEAD
} MyParserObject;

typedef struct BuiltinObject_ {
	OBJECT_HEAD
	int args_n;
	void (*in_func)(int args_n, Value *args);
} BuiltinObject;

typedef struct ModuleObject_ {
	PARSER_HEAD
	VectorInt else_e;
	VectorInt break_e;
	VectorInstr for_e;
} ModuleObject;

typedef struct FunctionObject_ {
	PARSER_HEAD
	MyParserObject *prev;
	int args_num;
} FunctionObject;

typedef struct ClassCodeObject_ {
	PARSER_HEAD
	int super_pos;
} ClassCodeObject;

typedef struct InstanceObject_ {
	OBJECT_HEAD
	VectorField fields;
} InstanceObject;

typedef struct MethodObject_ {
	OBJECT_HEAD
	FunctionObject *func;
	InstanceObject *inst;
} MethodObject;

void mark_class(MyObject *obj);
void free_class(MyObject *obj);
void mark_string(MyObject *obj);
void free_string(MyObject *obj);
void mark_list(MyObject *obj);
void free_list(MyObject *obj);
void mark_map(MyObject *obj);
void free_map(MyObject *obj);
void mark_module(MyObject *obj);
void free_module(MyObject *obj);
void mark_function(MyObject *obj);
void free_function(MyObject *obj);
void mark_class_code(MyObject *obj);
void free_class_code(MyObject *obj);
void mark_method(MyObject *obj);
void free_method(MyObject *obj);
void mark_builtin(MyObject *obj);
void free_builtin(MyObject *obj);

ClassObject G_class_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 8, mark_class, free_class, NULL, NULL };
ClassObject G_string_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 3, mark_string, free_string, NULL, NULL };
ClassObject G_list_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 4, mark_list, free_list, NULL, NULL };
ClassObject G_map_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 5, mark_map, free_map, NULL, NULL };
ClassObject G_module_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 9, mark_module, free_module, NULL, NULL };
ClassObject G_function_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 9, mark_function, free_function, NULL, NULL };
ClassObject G_class_code_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 9, mark_class_code, free_class_code, NULL,
		NULL };
ClassObject G_method_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 7, mark_method, free_method, NULL, NULL };

ClassObject G_builtin_class = { &G_class_class, 1, NULL, { NULL, 0, 0 }, 11, mark_builtin, free_builtin, NULL, NULL };

VectorString G_str_t; /*全局静态字符串表*/
GlobalInfo G_info;

void gc_mark() {
	printf("start  mark");
	int s;
	for (s = 0; s < G_str_t.n; s++) {
		StringObject *v = G_str_t.data[s];
		v->mark = 1;
	}
	G_info.top_parser->in_class->mark_obj((MyObject*) G_info.top_parser);
	Frame frame;
	int i;
	for (i = 0; i < G_info.top_frame; i++) {
		frame = G_info.frames[i];
		int m;
		for (m = 0; m < frame.n; m++) {
			Value v = frame.value[m];
			if (v.type == OBJECT) {
				if (v.u.object->mark == 0) {
					v.u.object->in_class->mark_obj(v.u.object);
				}
			}
		}
		int n;
		for (n = 0; n < frame.runtime.n; n++) {
			Value v = frame.runtime.data[n];
			if (v.type == OBJECT) {
				if (v.u.object->mark == 0) {
					v.u.object->in_class->mark_obj(v.u.object);
				}
			}
		}
	}
}

void gc_sweep() {
	printf("start sweep");
	MyObject *prev, *tmp, *curr;
	prev = curr = G_info.gc_head;
	if (curr != NULL) {
		if (curr->mark == 1) {
			curr->mark = 0;
			curr = curr->next;
		} else {
			tmp = curr;
			curr->in_class->free_obj(curr);
			curr = tmp->next;
			free(tmp);
		}
	}
	while (curr != NULL) {
		if (curr->mark == 1) {
			prev = curr;
			curr->mark = 0;
			curr = curr->next;
		} else {
			tmp = curr;
			curr->in_class->free_obj(curr);
			curr = tmp->next;
			prev->next = curr;
			free(tmp);
		}
	}
}

void gc_start() {
	printf("start gc ,count is  %d,capacity is %d", G_info.gc_count, G_info.gc_capacity);
	gc_mark();
	gc_sweep();
}

MyObject* gc_malloc(int size) {
	if (G_info.gc_count >= G_info.gc_capacity) {
		G_info.gc_capacity = G_info.gc_capacity * 2;
		gc_start();
	}
	MyObject *p = (MyObject*) malloc(size);
	if (p == NULL) {
		gc_start();
		p = (MyObject*) malloc(size);
		if (p == NULL) {
			printf("not enough memory");
			exit(0);
		}
	}
	G_info.gc_count++;
	p->mark = 0;
	p->next = G_info.gc_head;
	G_info.gc_head = p;
	return p;
}

void mark_vector_fields(VectorField v) {
	int i = 0;
	for (i = 0; i < v.n; i++) {
		if (v.data[i].value.type == OBJECT) {
			MyObject *obj = v.data[i].value.u.object;
			obj->in_class->mark_obj(obj);
		}
	}
}

void mark_string(MyObject *obj) {
	((StringObject*) obj)->mark = 1;
}

void free_string(MyObject *obj) {
	StringObject *s = (StringObject*) obj;
	if (s->is_const == -1) {
		free(s->str_char);
	}
}

StringObject* init_string_object(char *str_char, int pos) {
	StringObject *s = (StringObject*) gc_malloc(sizeof(StringObject));
	s->in_class = &G_string_class;
	int len = strlen(str_char);
	s->str_char = str_char;
	s->len = len;
	s->is_const = pos;
	return s;
}

int get_const_string(char *name) {
	int i;
	for (i = G_str_t.n - 1; i >= 0; i--) {
		StringObject *v = G_str_t.data[i];
		if (strcmp(v->str_char, name) == 0) {
			return i;
		}
	}
	return -1;
}
int save_const_string(char *name) {
	int pos = get_const_string(name);
	if (pos != -1) {
		return pos;
	}
	StringObject *s_o = init_string_object(strdup(name), G_str_t.n);
	vector_push(&G_str_t, StringObject*, s_o);
	return G_str_t.n - 1;
}

void mark_vector_value(VectorValue v) {
	int i = 0;
	for (i = 0; i < v.n; i++) {
		if (v.data[i].type == OBJECT) {
			MyObject *obj = v.data[i].u.object;
			obj->in_class->mark_obj(obj);
		}
	}
}
void mark_list(MyObject *obj) {
	((ListObject*) obj)->mark = 1;
	mark_vector_value(((ListObject*) obj)->list);
}

void free_list(MyObject *obj) {
	ListObject *l_obj = (ListObject*) obj;
	free(l_obj->list.data);
}

ListObject* init_list_object(int size) {
	ListObject *obj = (ListObject*) gc_malloc(sizeof(ListObject));
	obj->in_class = &G_list_class;
	vector_init(&obj->list, Value, size);
	return obj;
}

void mark_map(MyObject *obj) {
	MapObject *m_obj = (MapObject*) obj;
	m_obj->mark = 1;
	int i;
	for (i = 0; i < m_obj->capacity; i++) {
		HashNode *tmp;
		for (tmp = m_obj->hash_table[i]; tmp != NULL; tmp = tmp->next) {
			if (tmp->v.type == OBJECT) {
				MyObject *obj = tmp->v.u.object;
				obj->in_class->mark_obj(obj);
			}
			tmp->string->in_class->mark_obj((MyObject*) tmp->string);
		}
	}
}
void free_map(MyObject *obj) {
	MapObject *m_obj = (MapObject*) obj;
	int i;
	for (i = 0; i < m_obj->capacity; i++) {
		HashNode *head = m_obj->hash_table[i];
		HashNode *tmp;
		while (head != NULL) {
			tmp = head;
			head = tmp->next;
			free(tmp);
		}
	}
	free(m_obj->hash_table);
}

MapObject* init_map_object(int size) {
	MapObject *o = (MapObject*) gc_malloc(sizeof(MapObject));
	o->in_class = &G_map_class; /*后续加上这个列表对应的类类型*/
	o->capacity = size;
	o->hash_table = (HashNode**) malloc(sizeof(HashNode*) * size);
	int i;
	for (i = 0; i < size; i++) {
		o->hash_table[i] = NULL;
	}
	return o;
}

HashNode* init_hash_node(StringObject *str, Value v, int hash_value, HashNode *next) {
	HashNode *h = (HashNode*) malloc(sizeof(HashNode));
	h->hash_value = hash_value;
	h->string = str;
	h->v = v;
	h->next = next;
	return h;
}

int util_key2index(char *key, int b) {
	int sum = 0;
	int i;
	for (i = 0; i < strlen(key); i++) {
		sum = sum + (int) key[i];
	}
	return sum % b;
}

HashNode* get_hash_node(HashNode **table, StringObject *str, int hash_value) {
	HashNode *tmp;
	for (tmp = table[hash_value]; tmp != NULL; tmp = tmp->next) {
		if (tmp->string->len == str->len && strcmp(tmp->string->str_char, str->str_char) == 0) {
			return tmp;
		}
	}
	return NULL;
}

HashNode* save_hash_node(HashNode **table, Value v, StringObject *str, int hash_value) {
	HashNode *node = get_hash_node(table, str, hash_value);
	if (node != NULL) {
		return node;
	}
	table[hash_value] = init_hash_node(str, v, hash_value, table[hash_value]);
	return table[hash_value];
}

void mark_class(MyObject *obj) {
	ClassObject *c_obj = ((ClassObject*) obj);
	c_obj->mark = 1;
	mark_vector_fields(c_obj->fields);
	c_obj->super->mark_obj((MyObject*) c_obj->in_class);
}

void free_class(MyObject *obj) {
	ClassObject *c_obj = ((ClassObject*) obj);
	free(c_obj->fields.data);
}

void mark_instance(MyObject *obj) {
	InstanceObject *i_obj = ((InstanceObject*) obj);
	i_obj->mark = 1;
	mark_vector_fields(i_obj->fields);
	i_obj->in_class->mark_obj((MyObject*) i_obj->in_class);
}

void free_instance(MyObject *obj) {
	ClassObject *i_obj = ((ClassObject*) obj);
	free(i_obj->fields.data);
}

InstanceObject* init_instance_object(ClassObject *in_class) {
	InstanceObject *c_o = (InstanceObject*) gc_malloc(sizeof(InstanceObject));
	vector_new(&c_o->fields, Field);
	c_o->in_class = in_class;
	return c_o;
}

ClassObject* init_class_object(ClassCodeObject *code, StringPos name) {
	ClassObject *c_o = (ClassObject*) gc_malloc(sizeof(ClassObject));
	c_o->in_class = &G_class_class;
	vector_new(&c_o->fields, Field);
	c_o->name = name;
	c_o->mark_obj = mark_instance;
	c_o->free_obj = free_instance;
	c_o->code = code;
	c_o->super = NULL;
	return c_o;
}

void mark_module(MyObject *obj) {
	ModuleObject *m_obj = ((ModuleObject*) obj);
	m_obj->mark = 1;
	mark_vector_value(m_obj->value);
}

void free_module(MyObject *obj) {
	ModuleObject *m_obj = ((ModuleObject*) obj);
	free(m_obj->in_code.data);
	free(m_obj->value.data);
	free(m_obj->var.data);
	free(m_obj->else_e.data);
	free(m_obj->break_e.data);
	free(m_obj->for_e.data);
}

ModuleObject* init_module_object(StringPos name_pos) {
	ModuleObject *o = (ModuleObject*) gc_malloc(sizeof(ModuleObject));
	o->in_class = &G_module_class;
	vector_new(&o->in_code, Instruction);
	vector_new(&o->value, Value);
	vector_new(&o->var, Var);
	o->top = o;
	o->name = name_pos;
	vector_new(&o->else_e, int);
	vector_new(&o->break_e, int);
	vector_new(&o->for_e, Instruction);
	return o;
}

void mark_function(MyObject *obj) {
	FunctionObject *f_obj = ((FunctionObject*) obj);
	f_obj->mark = 1;
	mark_vector_value(f_obj->value);
}

void free_function(MyObject *obj) {
	FunctionObject *f_obj = ((FunctionObject*) obj);
	free(f_obj->in_code.data);
	free(f_obj->value.data);
	free(f_obj->var.data);
}

FunctionObject* init_function_object(ModuleObject *top, StringPos name, MyParserObject *prev) {
	FunctionObject *o = (FunctionObject*) gc_malloc(sizeof(FunctionObject));
	o->in_class = &G_function_class;
	vector_new(&o->in_code, Instruction);
	vector_new(&o->value, Value);
	vector_new(&o->var, Var);
	o->name = name;
	o->top = top;
	o->prev = prev;
	o->args_num = 0;
	return o;
}

void mark_class_code(MyObject *obj) {
	ClassCodeObject *c_obj = ((ClassCodeObject*) obj);
	c_obj->mark = 1;
	mark_vector_value(c_obj->value);
}

void free_class_code(MyObject *obj) {
	ClassCodeObject *c_obj = ((ClassCodeObject*) obj);
	free(c_obj->in_code.data);
	free(c_obj->value.data);
	free(c_obj->var.data);
}

ClassCodeObject* init_class_code_object(ModuleObject *top, StringPos name) {
	ClassCodeObject *o = (ClassCodeObject*) gc_malloc(sizeof(ClassCodeObject));
	o->in_class = &G_class_code_class;
	vector_new(&o->in_code, Instruction);
	vector_new(&o->value, Value);
	vector_new(&o->var, Var);
	o->name = name;
	o->super_pos = -1;
	o->top = top;
	return o;
}

void mark_method(MyObject *obj) {
	MethodObject *m_obj = ((MethodObject*) obj);
	m_obj->mark = 1;
	m_obj->func->in_class->mark_obj((MyObject*) m_obj->func);
}

void free_method(MyObject *obj) {
	NULL;/*do nothing */
}

MethodObject* init_method_object(FunctionObject *func, InstanceObject *inst) {
	MethodObject *o = (MethodObject*) gc_malloc(sizeof(MethodObject));
	o->in_class = &G_method_class;
	o->func = func;
	o->inst = inst;
	return o;
}

void mark_builtin(MyObject *obj) {
	BuiltinObject *b_obj = ((BuiltinObject*) obj);
	b_obj->mark = 1;
}

void free_builtin(MyObject *obj) {
	NULL;
}

ClassObject* value2class(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class == &G_class_class);
	return (ClassObject*) v.u.object;
}

ClassCodeObject* value2class_code(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class == &G_class_code_class);
	return (ClassCodeObject*) v.u.object;
}

Value obj2value(void *object) {
	Value v;
	v.type = OBJECT;
	v.u.object = (MyObject*) object;
	return v;
}
Value int2value(int i) {
	Value v;
	v.type = INTEGER;
	v.u.integer = i;
	return v;
}

FunctionObject* value2func(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class == &G_function_class);
	return (FunctionObject*) v.u.object;
}
ListObject* value2list(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class == &G_list_class);
	return (ListObject*) v.u.object;
}
int value2int(Value v) {
	assert(v.type == INTEGER);
	return v.u.integer;
}

InstanceObject* value2instance(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class->code != NULL);/*用code不等于空判断是否有问题*/
	return (InstanceObject*) v.u.object;
}
StringObject* value2string(Value v) {
	assert(v.type == OBJECT && v.u.object->in_class == &G_string_class);
	return (StringObject*) v.u.object;
}

void save_frame(Frame frame) {
	if (G_info.top_frame >= FRAME_DEPTH) {
		printf("stack over max depth");
		exit(0);
	}
	G_info.frames[G_info.top_frame] = frame;
	G_info.top_frame++;
}

Frame* top_frame() {
	assert(!(G_info.top_frame == 0));
	return &(G_info.frames[G_info.top_frame - 1]);
}

Frame* pop_frame() {
	assert(!(G_info.top_frame == 0));
	G_info.top_frame--;
	return &(G_info.frames[G_info.top_frame]);
}

Frame* buttom_frame() {
	assert(!(G_info.top_frame == 0));
	return &(G_info.frames[0]);
}

void parser_expr_main(MyParserObject *p, Lexer *l);
int parser_expr_list(MyParserObject *p, Lexer *l);
void parser_state_list(MyParserObject *p, Lexer *l);
void parser_state(MyParserObject *p, Lexer *l);
void parser_expr_map(MyParserObject *p, Lexer *l);
void parser_expr_array(MyParserObject *p, Lexer *l);
int parser_func_body(MyParserObject *p, Lexer *l);
ClassCodeObject* init_class_code_object(ModuleObject *top, StringPos name);
int get_loc_var(MyParserObject *p, StringPos name);
Value vm(MyParserObject *p);

void eat_token(Lexer *lex, int token_enum) {
	lex->type = token_enum;
	lex->curr_char = lex->content[++lex->pos];
}
void eat_token2(Lexer *lex, int token_enum) {
	lex->type = token_enum;
	lex->pos++;
	lex->curr_char = lex->content[++lex->pos];
}

void lexer_number(Lexer *lex) {
	for (; isdigit(lex->curr_char); next_char(lex)) {
		save_char(lex, lex->curr_char);
	}
	if (lex->curr_char == '.') {
		save_char(lex, lex->curr_char);
		next_char(lex);
		if (!isdigit(lex->curr_char)) {
			printf("token error");
			exit(1);
		};
		for (; isdigit(lex->curr_char); next_char(lex)) {
			save_char(lex, lex->curr_char);
		}
		save_char(lex, '\0');
		lex->type = TOKEN_NUMBER;
		lex->u.number = atof(lex->buff.data);
	} else {
		save_char(lex, '\0');
		lex->type = TOKEN_INTEGER;
		lex->u.integer = atoi(lex->buff.data);
	}
}

void lexer_string(Lexer *lex) {
	next_char(lex);
	for (; lex->curr_char != '"'; next_char(lex)) {
		save_char(lex, lex->curr_char);
	}
	save_char(lex, '\0');
	next_char(lex);
	int pos = save_const_string(lex->buff.data);
	lex->u.str_pos = pos;
	lex->type = TOKEN_STRING;
}

void lexer_identifier(Lexer *lex) {
	if (isalpha(lex->curr_char) || lex->curr_char == '_') {
		save_char(lex, lex->curr_char);
		next_char(lex);
	}
	for (; isalpha(lex->curr_char) || lex->curr_char == '_' || isdigit(lex->curr_char); next_char(lex)) {
		save_char(lex, lex->curr_char);
	}
	save_char(lex, '\0');
	int i;
	for (i = 0; i < NUM_RESERVED; i++) {
		if (strcmp(lex->buff.data, reserve[i]) == 0) {
			lex->type = i + 257;
			return;
		}
	}
	int pos = save_const_string(lex->buff.data);
	lex->u.str_pos = pos;
	lex->type = TOKEN_IDENTIFIER;
}

void next_token(Lexer *lex) {
	lex->type = TOKEN_EOF;
	lex->buff.n = 0;
	while (1) {
		switch (lex->curr_char) {
		case '\n': {
			next_char(lex);
			lex->curr_line++;
			lex->type = TOKEN_NEWLINE;
			break;
		}
		case ' ':
		case '\t':
		case '\v':
			next_char(lex);
			break;
		case '|': {
			if (look_char(lex)== '|') {
				eat_token2(lex, TOKEN_OR);
				return;
			} else {
				eat_token(lex, '|');
				return;
			}
		}
		case '&': {
			if (look_char(lex) == '&') {
				eat_token2(lex, TOKEN_AND);
				return;
			} else {
				eat_token(lex, '&');
				return;
			}
		}
		case '<': {
			if (look_char(lex) == '=') {
				eat_token2(lex, TOKEN_LT_EQ);
				return;
			} else {
				eat_token(lex, '<');
				return;
			}
		}
		case '>': {
			if (look_char(lex) == '=') {
				eat_token2(lex, TOKEN_GT_EQ);
				return;
			} else {
				eat_token(lex, '>');
				return;
			}
		}
		case '=': {
			if (look_char(lex) == '=') {
				eat_token2(lex, TOKEN_EQ);
				return;
			} else {
				eat_token(lex, '=');
				return;
			}
		}
		case '!': {
			if (look_char(lex) == '=') {
				eat_token2(lex, TOKEN_NOT_EQ);
				return;
			} else {
				eat_token(lex, '!');
				return;
			}
		}
		case '\0': {
			eat_token(lex, TOKEN_EOF);
			return;
		}
		case '"': {
			lexer_string(lex);
			return;
		}
		default: {
			if (isdigit(lex->curr_char)) {
				lexer_number(lex);
			} else if (isalpha(lex->curr_char) || lex->curr_char == '_') {
				lexer_identifier(lex);
			} else {
				eat_token(lex, lex->curr_char);
			}
			return;
		}
	};
}
}

void check_token(Lexer *l, int c) {
	if (l->type != c) {
		printf("type is an error has occur ");
		exit(1);
	}
}

void check_skip(Lexer *l, int c) {
	check_token(l, c);
	next_token(l);
}

int get_global_var(MyParserObject *p, StringPos name) {
	int r = -1;
	if (p->in_class == &G_module_class) {
		return r;
	}
	r = get_loc_var((MyParserObject*) p->top, name);
	return r;
}

int get_loc_var(MyParserObject *p, StringPos name) {
	int i;
	for (i = p->var.n - 1; i >= 0; i--) {
		Var v = p->var.data[i];
		if (v == name) {
			return i;
		}
	}
	return -1;
}

int save_loc_var(MyParserObject *p, StringPos name) {
	vector_push(&p->var, Var, name);
	return p->var.n - 1;
}

void save_incode(MyParserObject *p, OpKind op, int v) {
	Instruction in_code;
	in_code.op = op;
	in_code.v = v;
	vector_push(&p->in_code, Instruction, in_code);
}

void parser_load_var(MyParserObject *p, StringPos name) {
	int i = get_loc_var(p, name);
	if (i != -1) {
		save_incode(p, OP_LOAD, i);
		return;
	}
	i = get_global_var(p, name);
	if (i != -1) {
		save_incode(p, OP_LOAD_GLOBAL, i);
		return;
	}
	if (i == -1) {
		printf("can not find variable");
		exit(0);
	}
}

int parser_expr_funcall(MyParserObject *p, Lexer *l) {
	int n;
	check_skip(l, '(');
	if (l->type == ')') {
		n = 0;
	} else {
		n = parser_expr_list(p, l);
	}
	check_skip(l, ')');
	return n;
}

/*parser_expr_complex -> this |NAME |  '(' expr_mian')' {'.' NAME |'.' NAME '('[exp_list]')' | '[' binexpr ']' |  '('[exp_list]')' } */
/*此处如果有多余的字符会出现bug*/
ExprTempType parser_expr_complex(MyParserObject *p, Lexer *l) {
	ExprTempType e;
	e.flag = 0;
	e.str_pos = 0;
	if (l->type == TOKEN_IDENTIFIER) {
		e.flag = 1; /*说明是标识符*/
		e.str_pos = l->u.str_pos;
		next_token(l);
		if (l->type == '=') {
			return e;
		}
		parser_load_var(p, e.str_pos);
	} else if (l->type == '(') {
		next_token(l);
		parser_expr_main(p, l);
		check_skip(l, ')');
	} else if (l->type == TOKEN_THIS) {
		next_token(l);
		save_incode(p, OP_THIS, -1);
	} else if (l->type == TOKEN_SUPER) {
		next_token(l);
		save_incode(p, OP_SUPER, -1);
	}
	while (l->type == '.' || l->type == '[' || l->type == '(') {
		if (l->type == '.') {
			e.flag = 2;/*说明是成员变量*/
			next_token(l);
			check_token(l, TOKEN_IDENTIFIER);
			e.str_pos = l->u.str_pos;
			next_token(l);
			if (l->type == '=') {
				return e;
			}
			save_incode(p, OP_LOAD_ATTR, e.str_pos); /*这里直接从全局字符串中取字符串的位置，后续用字符串动态查找*/
		} else if (l->type == '[') {
			e.flag = 3; /*说明是索引*/
			next_token(l);
			parser_expr_main(p, l);
			check_skip(l, ']');
			if (l->type == '=') {
				return e;
			}
			save_incode(p, OP_INDEX, 0);
		} else {
			if (e.flag == 0) {
				printf("function call failed");
				exit(0);
			}
			save_incode(p, OP_FUNC_CALL, parser_expr_funcall(p, l));
		}
	}
	return e;
}

/*simpleexp ::= null  |  false  |  true  |  Number  |  String | map |  array  |  suffixedexp    | function func_body */
void parser_expr_base(MyParserObject *p, Lexer *l) {
	switch (l->type) {
	case TOKEN_NULL: {
		save_incode(p, OP_CONST_NULL, 0);
		next_token(l);
		break;
	}
	case TOKEN_TRUE: {
		save_incode(p, OP_CONST_TRUE, 0);
		next_token(l);
		break;
	}
	case TOKEN_FALSE: {
		save_incode(p, OP_CONST_FALSE, 0);
		next_token(l);
		break;
	}
	case TOKEN_NUMBER: {
		save_incode(p, OP_PUSH, p->value.n);
		Value v;
		v.type = NUMBER;
		v.u.number = l->u.number;
		save_value(p, v);
		next_token(l);
		break;
	}
	case TOKEN_INTEGER: {
		save_incode(p, OP_CONST_INT, l->u.integer);
		next_token(l);
		break;
	}
	case TOKEN_STRING: {
		save_incode(p, OP_PUSH, p->value.n);
		Value v = obj2value(G_str_t.data[l->u.str_pos]);/*从全局字符串表中获取字符串对象*/
		save_value(p, v);
		next_token(l);
		break;
	}
	case '[': {
		parser_expr_array(p, l);
		break;
	}
	case '{': {
		parser_expr_map(p, l);
		break;
	}
	case TOKEN_FUNCTION: {
		next_token(l);
		FunctionObject *f_o = init_function_object(p->top, 0, p); /*这里名字NULL可以改成-1*/
		if (p->in_class == &G_class_code_class) {
			save_loc_var((MyParserObject*) f_o, 2);/*类方法第一变量是this*/
		}
		int args_num = parser_func_body((MyParserObject*) f_o, l);
		f_o->args_num = args_num;
		save_incode(p, OP_PUSH, p->value.n);
		Value v = obj2value(f_o);
		save_value(p, v);
		break;
	}
	default: {
		parser_expr_complex(p, l);
	}
	}
}

/*prefixexp -> [simpleexp | unop prefixexp] */
void parser_expr_prefix(MyParserObject *p, Lexer *l) {
	if (l->type == '!' || l->type == '-') {
		int op = l->type;
		next_token(l);
		parser_expr_prefix(p, l);
		if (op == '!') {
			save_incode(p, OP_NOT, 0);
		} else if (op == '-') {
			save_incode(p, OP_MINUS, 0);
		}
	} else {
		parser_expr_base(p, l);
	}
}

/*binexpr ->   prefixexp { binop prefixexp} */
void parser_mul_div(MyParserObject *p, Lexer *l) {
	parser_expr_prefix(p, l);
	while (l->type == '*' || l->type == '/') {
		int op = l->type;
		next_token(l);
		if (op == '*') {
			parser_expr_prefix(p, l);
			save_incode(p, OP_MUL, 0);
		} else {
			parser_expr_prefix(p, l);
			save_incode(p, OP_DIV, 0);
		}
	}
}

void parser_plus_minus(MyParserObject *p, Lexer *l) {
	parser_mul_div(p, l);
	while (l->type == '+' || l->type == '-') {
		int op = l->type;
		next_token(l);
		if (op == '+') {
			parser_mul_div(p, l);
			save_incode(p, OP_PLUS, 0);
		} else {
			parser_mul_div(p, l);
			save_incode(p, OP_SUB, 0);
		}
	}
}
void parser_boolean(MyParserObject *p, Lexer *l) {
	parser_plus_minus(p, l);
	while (l->type == '>' || l->type == '<' || l->type == TOKEN_LT_EQ || l->type == TOKEN_GT_EQ) {
		int op = l->type;
		next_token(l);
		if (op == '>') {
			parser_plus_minus(p, l);
			save_incode(p, OP_GT, 0);
		} else if (op == '<') {
			parser_plus_minus(p, l);
			save_incode(p, OP_LT, 0);
		} else if (op == TOKEN_LT_EQ) {
			parser_plus_minus(p, l);
			save_incode(p, OP_LT_EQ, 0);
		} else {
			parser_plus_minus(p, l);
			save_incode(p, OP_GT_EQ, 0);
		}
	}
}

void parser_equal(MyParserObject *p, Lexer *l) {
	parser_boolean(p, l);
	while (l->type == TOKEN_EQ || l->type == TOKEN_NOT_EQ) {
		int op = l->type;
		next_token(l);
		if (op == TOKEN_EQ) {
			parser_boolean(p, l);
			save_incode(p, OP_EQ, 0);
		} else {
			parser_boolean(p, l);
			save_incode(p, OP_NOT_EQ, 0);
		}
	}
}

void parser_logical_and(MyParserObject *p, Lexer *l) {
	parser_equal(p, l);
	while (l->type == TOKEN_AND) {
		next_token(l);
		parser_equal(p, l);
		save_incode(p, OP_AND, 0);
	};
}

void parser_expr_main(MyParserObject *p, Lexer *l) {
	parser_logical_and(p, l);
	while (l->type == TOKEN_OR) {
		next_token(l);
		parser_logical_and(p, l);
		save_incode(p, OP_OR, 0);
	};
}

/*map -> '{' [ Name ':' exp {',' Name ':' exp} ] '}'*/
void parser_expr_map(MyParserObject *p, Lexer *l) {
	check_skip(l, '{');
	int n = 0;
	while (l->type == TOKEN_IDENTIFIER || l->type == TOKEN_STRING) {
		save_incode(p, OP_PUSH, p->value.n);
		Value v = obj2value(G_str_t.data[l->u.str_pos]);/*从全局字符串表中获取字符串对象*/
		save_value(p, v);
		next_token(l);
		if (l->type == ':') {
			next_token(l);
			parser_expr_main(p, l);
		} else {
			printf("map parser error");
			exit(0);
		}
		n++;
		if (l->type == ',') {
			next_token(l);
		} else if (l->type == '}') {
			break;
		} else {
			printf("map parser error  ,");
			exit(0);
		}
	}
	check_skip(l, '}');
	save_incode(p, OP_BUILD_MAP, n); /*这里一个key-value的值在函数运行栈中写了两次，在创建这里n应乘2*/
}

/*array -> '[' [exp_list] ']'*/
void parser_expr_array(MyParserObject *p, Lexer *l) {
	int n;
	check_skip(l, '[');
	if (l->type == ']') {
		n = 0;
	} else {
		n = parser_expr_list(p, l);
	}
	save_incode(p, OP_BUILD_LIST, n);
	check_skip(l, ']');
}

/*new -> new  NAME '(' exp_list ')'*/
void parser_expr_new(MyParserObject *p, Lexer *l) {
	int index = get_loc_var((MyParserObject*) p->top, l->u.str_pos);
	if (index == -1) {
		printf("new error");
		exit(0);
	}
	save_incode(p, OP_LOAD, index);
	next_token(l);
	int n;
	check_skip(l, '(');
	if (l->type == ')') {
		n = 0;
	} else {
		n = parser_expr_list(p, l);
	}
	check_skip(l, ')');
	save_incode(p, OP_BUILD_OBJECT, n);
}

/*parser_expr_assignment -> suffixexp | assignment*/
/*assignment ::= suffixedexp = binexpr|  new */
void parser_expr_assignment(MyParserObject *p, Lexer *l) {
	ExprTempType e = parser_expr_complex(p, l);
	if (l->type != '=') {
		return;
	}
	next_token(l);
	if (l->type == TOKEN_NEW) {
		next_token(l);
		parser_expr_new(p, l);
	} else {
		parser_expr_main(p, l);
	}
	switch (e.flag) {
	case 1: {
		int i = get_loc_var(p, e.str_pos);
		if (i == -1) {
			i = save_loc_var(p, e.str_pos);
		}
		save_incode(p, OP_STORE, i);
		break;
	}
	case 2: {
		save_incode(p, OP_STORE_ATTR, e.str_pos);
		break;
	}
	case 3: {
		save_incode(p, OP_STORE_INDEX, e.str_pos);
		break;
	}
	default: {
		printf(" assignment error  ");
		exit(1);
	}
	}
}

/*parser_expr_list ->  exp {',' exp } */
int parser_expr_list(MyParserObject *p, Lexer *l) {
	int n = 1;
	parser_expr_main(p, l);
	while (l->type == ',') {
		next_token(l);
		parser_expr_main(p, l);
		n++;
	}
	return n;
}

/* block -> '{'state_list'}' */
void parser_block(MyParserObject *p, Lexer *l) {
	check_skip(l, '{');
	parser_state_list(p, l);
	check_skip(l, '}');
}

/*parser_while ->  while '(' exp ')' block*/
void parser_while(MyParserObject *p, Lexer *l) {
	check_skip(l, '(');
	vector_push(&p->top->break_e, int, -1);
	int mark0 = p->in_code.n;
	parser_expr_main(p, l);
	int mark1 = p->in_code.n;
	save_incode(p, OP_GOTO_IF_FALSE, 0);
	check_skip(l, ')');
	parser_block(p, l);
	save_incode(p, OP_GOTO, mark0);
	(p->in_code.data[mark1]).v = p->in_code.n;
	int mark2 = vector_pop(&p->top->break_e);
	if (mark2 != -1) {
		(p->in_code.data[mark2]).v = p->in_code.n;
	}
}

void parser_for_sub(MyParserObject *p, Lexer *l) {
	int mark0 = p->in_code.n;
	int mark1 = -1;
	int mark2 = -1;
	int num = -1;
	if (l->type == ';') {
		next_token(l);
	} else {
		parser_expr_main(p, l);
		mark1 = p->in_code.n;
		save_incode(p, OP_GOTO_IF_FALSE, 0);
		check_skip(l, ';');
	}
	if (l->type == ')') {
		next_token(l);
	} else {
		int top = p->in_code.n;
		mark2 = p->top->for_e.n;
		parser_expr_assignment(p, l);
		while (l->type == ',') {
			next_token(l);
			parser_expr_assignment(p, l);
		}
		num = p->in_code.n - top;
		int i;
		for (i = 0; i < num; i++) {
			vector_push(&p->top->for_e, Instruction, (p->in_code.data)[top + i]);
		}
		p->in_code.n = top;
		check_skip(l, ')');
	}
	parser_block(p, l);
	if (mark2 != -1) {
		int i;
		for (i = 0; i < num; i++) {
			vector_push(&p->in_code, Instruction, (p->top->for_e.data)[mark2 + i]);
		}
		p->top->for_e.n = mark2;
	}
	save_incode(p, OP_GOTO, mark0);
	if (mark1 != -1) {
		(p->in_code.data[mark1]).v = p->in_code.n;
	}

}

/*parser_for ->  for '(' [Name '=' exp ]';' [exp] ';'[exp {',' exp}] ')' block*/
/*parser_for ->  for '(' Name of exp ')'  block */
void parser_for(MyParserObject *p, Lexer *l) {
	assert(p->in_class != &G_class_code_class);
	check_skip(l, '(');
	vector_push(&p->top->break_e, int, -1);
	if (l->type == TOKEN_IDENTIFIER) {
		StringPos name = l->u.str_pos;
		next_token(l);
		if (l->type == TOKEN_OF) { /*parser_for_of */
			next_token(l);
			parser_expr_main(p, l);
			check_skip(l, ')');
			int var = save_loc_var(p, name);
			int array_point = save_loc_var(p, 0);
			save_incode(p, OP_STORE, array_point);
			save_incode(p, OP_LOAD, array_point);
			save_incode(p, OP_ARRAY_LENGTH, 0);
			int array_length = save_loc_var(p, 0);
			save_incode(p, OP_STORE, array_length);
			save_incode(p, OP_CONST_INT, 0);
			int counter = save_loc_var(p, 0);
			save_incode(p, OP_STORE, counter);
			int mark0 = p->in_code.n;
			save_incode(p, OP_LOAD, counter);
			save_incode(p, OP_LOAD, array_length);
			save_incode(p, OP_LT, 0);
			int mark1 = p->in_code.n;
			save_incode(p, OP_GOTO_IF_FALSE, 0);
			save_incode(p, OP_LOAD, array_point);
			save_incode(p, OP_LOAD, counter);
			save_incode(p, OP_INDEX, 0);
			save_incode(p, OP_STORE, var);
			parser_block(p, l);
			save_incode(p, OP_PLUS_SELF, counter);
			save_incode(p, OP_GOTO, mark0);
			(p->in_code.data[mark1]).v = p->in_code.n;
		} else if (l->type == '=') {
			next_token(l);
			parser_expr_main(p, l);
			int var = save_loc_var(p, name);
			save_incode(p, OP_STORE, var);
			check_skip(l, ';');
			parser_for_sub(p, l);
		} else {
			printf("type is an error has occur ");
			exit(1);
		}
	} else if (l->type == ';') {
		next_token(l);
		parser_for_sub(p, l);
	} else {
		printf("type is an error has occur ");
		exit(1);
	}
	int mark = vector_pop(&p->top->break_e);
	if (mark != -1) {
		(p->in_code.data[mark]).v = p->in_code.n;
	}
}

/*parser_class_state ->  { Name '=' exp [';'|'\n'] | Name func_body [';'|'\n']}*/
void parser_class_state(MyParserObject *p, Lexer *l) {
	while (l->type == TOKEN_IDENTIFIER) {
		StringPos name = l->u.str_pos;
		int pos = save_loc_var(p, name);
		next_token(l);
		if (l->type == '=') {
			next_token(l);
			parser_expr_main(p, l);
			save_incode(p, OP_STORE, pos);
		} else if (l->type == '(') {
			FunctionObject *f_o = init_function_object(p->top, name, p);
			save_loc_var((MyParserObject*) f_o, 2);/*类方法第一变量是this*/
			int args_num = parser_func_body((MyParserObject*) f_o, l);
			f_o->args_num = args_num;
			save_incode(p, OP_PUSH, p->value.n);
			Value v;
			v.type = OBJECT;
			v.u.object = (MyObject*) f_o;
			save_value(p, v);
			save_incode(p, OP_STORE, pos);
		} else {
			printf("type is an error has occur ");
			exit(1);
		}
		if (l->type == ';') {
			next_token(l);
		}
	}
}
/*parser_class ->  class Name [extends Name ] '{' class_state '}'*/
void parser_class(MyParserObject *p, Lexer *l) {
	check_token(l, TOKEN_IDENTIFIER);
	assert(p->in_class == &G_module_class);
	StringPos name = l->u.str_pos;
	ClassCodeObject *c_o = init_class_code_object(p->top, name);
	next_token(l);
	if (l->type == TOKEN_EXTENDS) {
		next_token(l);
		check_token(l, TOKEN_IDENTIFIER);
		int super_pos = get_loc_var(p, l->u.str_pos);
		if (super_pos != -1) {
			c_o->super_pos = super_pos;
		} else {
			printf("can not find superclass");
			exit(0);
		}
		next_token(l);
	}
	check_skip(l, '{');
	parser_class_state((MyParserObject*) c_o, l);
	check_skip(l, '}');
	save_incode(p, OP_PUSH, p->value.n);
	Value v;
	v.type = OBJECT;
	v.u.object = (MyObject*) c_o;
	save_value(p, v);
	save_incode(p, OP_BUILD_CLASS, save_loc_var(p, name));
}

/*func_body -> '(' [ Name {',' Name} ] ')' block  */
int parser_func_body(MyParserObject *p, Lexer *l) {
	int args_num = 0;
	check_skip(l, '(');
	if (l->type == TOKEN_IDENTIFIER) {
		save_loc_var(p, l->u.str_pos);
		next_token(l);
		args_num++;
		while (l->type == ',') {
			next_token(l);
			check_token(l, TOKEN_IDENTIFIER);
			save_loc_var(p, l->u.str_pos);
			next_token(l);
			args_num++;
		}
	}
	check_skip(l, ')');
	parser_block(p, l);
	Instruction in_code = vector_last(&p->in_code);
	if (in_code.op != OP_RETURN) {
		save_incode(p, OP_CONST_NULL, 0);
		save_incode(p, OP_RETURN, 0);
	}
	return args_num;
}
/*parser_func ->  function Name func_body */
void parser_function(MyParserObject *p, Lexer *l) {
	check_token(l, TOKEN_IDENTIFIER);
	StringPos name = l->u.str_pos;
	next_token(l);
	FunctionObject *f_o = init_function_object(p->top, name, p);
	int args_num = parser_func_body((MyParserObject*) f_o, l);
	f_o->args_num = args_num;
	save_incode(p, OP_PUSH, p->value.n);
	Value v;
	v.type = OBJECT;
	v.u.object = (MyObject*) f_o;
	save_value(p, v);
	save_incode(p, OP_STORE, save_loc_var(p, name));
}

/*parser_if ->  if '(' exp ')' block {else if '(' exp ')' block} [else block]*/
void parser_if(MyParserObject *p, Lexer *l) {
	assert(p->in_class != &G_class_code_class);
	check_skip(l, '(');
	parser_expr_main(p, l);
	/*jmpcode0是if表达式执行失败需跳转的执行jmp */
	int mark0 = p->in_code.n;
	save_incode(p, OP_GOTO_IF_FALSE, 0);
	check_skip(l, ')');
	parser_block(p, l);
	/*jmpcode1是if表达式语句执行成功后 跳转到最后结束的指令jmp */
	save_incode(p, OP_GOTO, p->in_code.n);
	(p->in_code.data[mark0]).v = p->in_code.n;
	int mark1 = p->in_code.n - 1;
	int flag = 0; /*else语句是否已经执行，如果执行后续将不在出现else语句*/
	int top = p->top->else_e.n;
	while (l->type == TOKEN_ELSE) {
		next_token(l);
		if (l->type == '{') {
			if (flag == 1) {
				printf("else is error");
				exit(1);
			}
			flag = 1;
			parser_block(p, l);
			break;
		}
		while (l->type == TOKEN_IF) {
			if (flag == 1) {
				printf("else if is error");
				exit(1);
			}
			next_token(l);
			check_skip(l, '(');
			parser_expr_main(p, l);
			/*jmpcode2是else if表达式语句执行失败需 跳转到下一个else if 的指令jmp */
			save_incode(p, OP_GOTO_IF_FALSE, p->in_code.n);
			int mark2 = p->in_code.n - 1;
			check_skip(l, ')');
			parser_block(p, l);
			(p->in_code.data[mark2]).v = p->in_code.n + 1;
			/*jmpcode3是记录了每次else if 执行成功后 需要跳转到结束的jmp指令*/
			int mark3 = p->in_code.n;
			/*这里设置else if如果执行结束直接跳到循环结束的位置，列表内存放每次else if结束的jmp的位置*/
			vector_push(&p->top->else_e, int, p->in_code.n); /*后续考虑不用顺序表保存这个jmp的位置*/
			save_incode(p, OP_GOTO, mark3);
			break;
		}
	}
	int i, pos;
	/*else if 列表循环赋值结束的位置*/
	for (i = top; i < p->top->else_e.n; i++) {
		pos = p->top->else_e.data[i];
		(p->in_code.data[pos]).v = p->in_code.n;
	}
	p->top->else_e.n = top;
	(p->in_code.data[mark1]).v = p->in_code.n;
}
void parser_break(MyParserObject *p, Lexer *l) {
	int mark = p->in_code.n;
	save_incode(p, OP_GOTO, 0);
	int top = p->top->break_e.n;
	if (top == 0) {
		printf("error break");
		exit(0);
	}
	(p->top->break_e.data)[top - 1] = mark;
}
void parser_return(MyParserObject *p, Lexer *l) {
	if (l->type == ';' || l->type == '}') {
		save_incode(p, OP_CONST_NULL, 0);
	} else {
		parser_expr_main(p, l);
	}
	save_incode(p, OP_RETURN, 0);
}

void parser_state(MyParserObject *p, Lexer *l) {
	switch (l->type) {
	case TOKEN_IF: {
		next_token(l);
		parser_if(p, l);
		break;
	}
	case TOKEN_WHILE: {
		next_token(l);
		parser_while(p, l);
		break;
	}
	case TOKEN_FOR: {
		next_token(l);
		parser_for(p, l);
		break;
	}
	case TOKEN_CLASS: {
		next_token(l);
		parser_class(p, l);
		break;
	}
	case TOKEN_FUNCTION: {
		next_token(l);
		parser_function(p, l);
		break;
	}
	case TOKEN_BREAK: {
		next_token(l);
		parser_break(p, l);
		break;
	}
	case TOKEN_RETURN: {
		next_token(l);
		parser_return(p, l);
		break;
	}
	default: {
		parser_expr_assignment(p, l);
		break;
	}
	}
}

/* state_list -> {state [';'|'\n']} [last_state] */
void parser_state_list(MyParserObject *p, Lexer *l) {
	while (l->type != TOKEN_EOF) {
		parser_state(p, l);
		if (l->type == ';') {
			next_token(l);
		}
		if (l->type == ')') {
			printf("error char");
			exit(0);
		}
		if (l->type == '}') {
			break;
		}
	}
}

/* main -> state_list EOF */
void parser_main(MyParserObject *p, Lexer *l) {
	next_token(l);
	parser_state_list(p, l);
	check_token(l, TOKEN_EOF);
}

Frame init_frame(int n) {
	Frame frame;
	frame.value = (Value*) malloc(sizeof(Value) * n);
	frame.n = n;
	int i;
	for (i = 0; i < n; i++) {
		frame.value[i].type = UNDEFINED;
	}
	vector_new(&frame.runtime, Value);
	return frame;
}

void free_frame(Frame *frame) {
	free(frame->value);
	frame->value = NULL;
	free(frame->runtime.data);
	frame->runtime.data = NULL;
}

#define switch_digit_eval(v1,v2,rtn)   \
case OP_EQ: {                          \
	v.type = (v1 == v2) ? TRUE : FALSE;\
	break;                             \
}                                      \
case OP_NOT_EQ: {                      \
	v.type = (v1 == v2) ? FALSE : TRUE;\
	break;                             \
}                                      \
case OP_GT: {                          \
	v.type = (v1 > v2) ? TRUE : FALSE; \
	break;                             \
}                                      \
case OP_LT: {                          \
	v.type = (v1 < v2) ? TRUE : FALSE; \
	break;                             \
}                                      \
case OP_LT_EQ: {                       \
	v.type = (v1 <= v2) ? TRUE : FALSE;\
	break;                             \
}                                      \
case OP_GT_EQ: {                       \
	v.type = (v1 >= v2) ? TRUE : FALSE;\
	break;                             \
}                                      \
case OP_AND: {                         \
	rtn = (v1 == 0) ? v1 : v2;         \
	break;                             \
}                                      \
case OP_OR: {                          \
	rtn = (v1 == 0) ? v2 : v1;         \
	break;                             \
}                                      \
case OP_PLUS: {                        \
	rtn = v1 + v2;                     \
	break;                             \
}                                      \
case OP_MINUS: {                       \
	rtn = v1 - v2;                     \
	break;                             \
}                                      \
case OP_MUL: {                         \
	rtn = v1 * v2;                     \
	break;                             \
}
Value vm_number_eval(double v1, double v2, OpKind op) {
	Value v;
	v.type = NUMBER;
	double rtn = 0;
	switch (op) {
	switch_digit_eval(v1, v2, rtn)
	case OP_DIV: {
		rtn = v1 / v2;
		break;
	}
	default: {
		printf("op error");
		exit(0);
	}
	}
	v.u.number = rtn;
	return v;
}
Value vm_integer_eval(int v1, int v2, OpKind op) {
	Value v;
	v.type = INTEGER;
	int rtn = 0;
	switch (op) {
	switch_digit_eval(v1, v2, rtn)
	case OP_DIV: {
		double d = v1 / v2;
		if ((int) d == d) {
			v.u.integer = (int) d;
		} else {
			v.type = NUMBER;
			v.u.number = d;
		}
		return v;
	}
	default: {
		printf("op error");
		exit(0);
	}
	}
	v.u.integer = rtn;
	return v;
}
Value vm_bool_eval(ValueKind v1, ValueKind v2, OpKind op) {
	Value v;
	v.type = FALSE;
	return v;
}
Value vm_string_eval(StringObject *v1, StringObject *v2, OpKind op) {
	char *v1_string = v1->str_char;
	char *v2_string = v2->str_char;
	char *tmp_string = (char*) malloc(strlen(v1_string) + strlen(v2_string) + 1);
	strcpy(tmp_string, v1_string);
	strcat(tmp_string, v2_string);
	StringObject *str = init_string_object(tmp_string, -1);
	return obj2value(str);
}
Value vm_list_eval(ListObject *v1, ListObject *v2, OpKind op) {
	Value v;
	/*do something*/
	return v;
}

void vm_binop_eval(Frame *frame, Instruction code) {
	Value v2 = vector_pop(&frame->runtime);
	Value v1 = vector_pop(&frame->runtime);
	Value v;
	switch (v1.type) {
	case NUMBER: {
		if (v2.type == NUMBER) {
			v = vm_number_eval(v1.u.number, v2.u.number, code.op);
		} else if (v2.type == INTEGER) {
			v = vm_number_eval(v1.u.number, (double) v2.u.integer, code.op);
		} else {
			printf("wrong type,unable calculate");
			exit(0);
		}
		break;
	}
	case INTEGER: {
		if (v2.type == INTEGER) {
			v = vm_integer_eval(v1.u.integer, v2.u.integer, code.op);
		} else if (v2.type == NUMBER) {
			v = vm_number_eval((double) v1.u.integer, v2.u.number, code.op);
		} else {
			printf("wrong type,unable calculate");
			exit(0);
		}
		break;
	}
	case TRUE:
	case FALSE: {
		if (v2.type == TRUE || v2.type == FALSE) {
			v = vm_bool_eval(v1.type, v2.type, code.op);
		} else {
			printf("wrong type,unable calculate");
			exit(0);
		}
		break;
	}
	case OBJECT: {
		if (v1.u.object->in_class == &G_string_class && v2.type == OBJECT && v2.u.object->in_class == &G_string_class) {
			v = vm_string_eval((StringObject*) v1.u.object, (StringObject*) v2.u.object, code.op);
		} else if (v1.u.object->in_class == &G_list_class && v2.type == OBJECT
				&& v2.u.object->in_class == &G_list_class) {
			v = vm_list_eval((ListObject*) v1.u.object, (ListObject*) v2.u.object, code.op);
		} else {
			printf("wrong type,unable calculate");
			exit(0);
		}
		break;
	}
	default: {
		printf("wrong type,unable calculate");
		exit(0);
	}
	}
	save_runtime_stack(frame, v);
}

void vm_build_list(Frame *frame, Instruction code) {
	ListObject *l_o = init_list_object(code.v);
	int i;
	for (i = 0; i < code.v; i++) {
		l_o->list.data[code.v - i - 1] = vector_pop(&frame->runtime);
	}
	l_o->list.n = code.v;
	save_runtime_stack(frame, obj2value(l_o));
}
void vm_array_length(Frame *frame) {
	Value v_list = vector_pop(&frame->runtime);
	ListObject *l_o = value2list(v_list);
	save_runtime_stack(frame, int2value(l_o->list.n));
}
void vm_index(Frame *frame) {
	Value v_n = vector_pop(&frame->runtime);
	Value v_l = vector_pop(&frame->runtime);
	if (is_map_string(v_l, v_n)) {
		MapObject *m_o = (MapObject*) v_l.u.object;
		StringObject *str = (StringObject*) v_n.u.object;
		int hash_value = util_key2index(str->str_char, m_o->capacity);
		HashNode *node = get_hash_node(m_o->hash_table, str, hash_value);
		if (node == NULL) {
			printf("can not find map index");
			exit(0);
		}
		save_runtime_stack(frame, node->v);
	} else if (is_list_integer(v_l, v_n)) {
		if (v_n.u.integer >= ((ListObject*) v_l.u.object)->list.n) {
			printf("index length too long");
			exit(0);
		};
		save_runtime_stack(frame, ((ListObject* ) v_l.u.object)->list.data[v_n.u.integer]);
	} else {
		printf("error in vm_index");
		exit(0);
	}
}
void vm_func_call(Frame *frame, Instruction code) {
	int pos = frame->runtime.n - code.v - 1;
	Value *p = &(frame->runtime.data[pos]);
	assert(p->type == OBJECT);
	if (p->u.object->in_class == &G_function_class) {
		FunctionObject *f_o = (FunctionObject*) p->u.object;
		if (code.v != f_o->args_num) {
			printf("function call args error2");
			exit(0);
		}
		Frame curr_frame = init_frame(f_o->var.n);
		int i;
		for (i = 0; i < code.v; i++) {
			curr_frame.value[i] = (frame->runtime.data)[frame->runtime.n - code.v + i];
		}
		save_frame(curr_frame);
		Value rtn = vm((MyParserObject*) f_o);/*需要优化*/
		/*执行结束后释放frame里面内容，并让frame_stack减1 free(curr_frame)*/
		Frame *top_frame = pop_frame();
		free_frame(top_frame);
		/*当前运行栈中回退到调用之前的位置*/
		frame->runtime.n = frame->runtime.n - code.v - 1;
		/*把return的值插入到当前运行栈的栈顶*/
		save_runtime_stack(frame, rtn);
	} else if (p->u.object->in_class == &G_builtin_class) {
		BuiltinObject *b_o = (BuiltinObject*) p->u.object;
		if (code.v != b_o->args_n) {
			printf("function call args error3");
			exit(0);
		}
		b_o->in_func(b_o->args_n, p + 1);
		frame->runtime.n = frame->runtime.n - code.v - 1;
	} else if (p->u.object->in_class == &G_method_class) {
		MethodObject *m_o = (MethodObject*) p->u.object;
		FunctionObject *f_o = m_o->func;
		InstanceObject *i_o = m_o->inst;
		if (code.v != f_o->args_num) {
			printf("function call args error2");
			exit(0);
		}
		Frame curr_frame = init_frame(f_o->var.n + 1);
		curr_frame.value[0] = obj2value(i_o);
		int i;
		for (i = 0; i < code.v; i++) {
			curr_frame.value[i + 1] = (frame->runtime.data)[frame->runtime.n - code.v + i];
		}
		save_frame(curr_frame);
		Value rtn = vm((MyParserObject*) f_o);
		Frame *top_frame = pop_frame();
		free_frame(top_frame);
		frame->runtime.n = frame->runtime.n - code.v - 1;
		save_runtime_stack(frame, rtn);
	} else {
		printf("function call error4");
		exit(0);
	}
}
void vm_build_class(Frame *frame, Instruction code) {
	Value v_code = vector_pop(&frame->runtime);
	ClassCodeObject *code_o = value2class_code(v_code);
	save_frame(init_frame(code_o->var.n));
	vm((MyParserObject*) code_o);/*这里执行完成后会在顶层frame里面放入执行后的值，把这个值与后续的classObject关联起来*/
	Frame *class_frame = pop_frame();/*free(class_frame)*/
	ClassObject *c_o = init_class_object(code_o, code_o->name);
	Field fields;
	int i;
	for (i = 0; i < code_o->var.n; i++) {
		fields.var = code_o->var.data[i];
		fields.value = class_frame->value[i];
		save_field(c_o, fields);
	}
	if (code_o->super_pos != -1) {
		Frame *g_frame = buttom_frame();
		c_o->super = value2class(g_frame->value[code_o->super_pos]);
	}
	frame->value[code.v] = obj2value(c_o);
	free_frame(class_frame);
}

void vm_store_index(Frame *frame, Instruction code) {
	Value v_value = vector_pop(&frame->runtime);
	Value v_index = vector_pop(&frame->runtime);
	Value v_list = vector_pop(&frame->runtime);
	int idx = value2int(v_index);
	ListObject *l_o = value2list(v_list);
	int d_value = idx + 1 - l_o->list.n;
	Value v;
	v.type = NONE;
	if (d_value > 0) {
		int i;
		for (i = 0; i < d_value; i++) {
			vector_push(&l_o->list, Value, v);
		}
	}
	l_o->list.data[idx] = v_value;
	save_runtime_stack(frame, v_list);
}

int get_class_field(ClassObject *o, Var var, Value *v) {
	ClassObject *tmp;
	int i;
	Field field;
	for (tmp = o; tmp != NULL; tmp = tmp->super) {
		for (i = tmp->fields.n - 1; i >= 0; i--) {
			field = tmp->fields.data[i];
			if (field.var == var) {
				*v = field.value;
				return 1;
			}
		}
	}
	return 0;
}

int get_instance_field(InstanceObject *i_o, Var var, Value *v) {
	int i;
	Field field;
	for (i = i_o->fields.n - 1; i >= 0; i--) {
		field = i_o->fields.data[i];
		if (field.var == var) {
			*v = field.value;
			return 1;
		}
	}
	return 0;
}

void vm_load_attr(Frame *frame, Var var) {
	Value v = vector_pop(&frame->runtime);
	assert(v.type == OBJECT);
	if (v.u.object->in_class == &G_map_class) {
		MapObject *m_o = (MapObject*) v.u.object;
		StringObject *str = G_str_t.data[var];
		int hash_value = util_key2index(str->str_char, m_o->capacity);
		HashNode *node = get_hash_node(m_o->hash_table, str, hash_value);
		if (node == NULL) {
			printf("can not find map index");
			exit(0);
		}
		save_runtime_stack(frame, node->v);
	} else if (v.u.object->in_class == &G_class_class) {
		Value this_v = frame->value[0];/*这里是方法内第一个值，指向自己的this*/
		ClassObject *c_o = (ClassObject*) v.u.object;
		InstanceObject *this_o = value2instance(this_v);
		Value v_field;
		if (get_class_field(c_o, var, &v_field) == 0) {
			printf("can not find class field");
			exit(0);
		}
		if (is_func_obj(v_field)) {
			MethodObject *m_o = init_method_object((FunctionObject*) v_field.u.object, this_o);
			v_field.u.object = (MyObject*) m_o;
			save_runtime_stack(frame, v_field);
		} else {
			save_runtime_stack(frame, v_field);/*  后续使用内存拷贝的方式*/
		}
	} else if (v.u.object->in_class->code != NULL) {
		InstanceObject *i_o = (InstanceObject*) v.u.object;
		Value v_field;
		if (get_instance_field(i_o, var, &v_field) == 0
				&& get_class_field((ClassObject*) i_o->in_class, var, &v_field) == 0) {
			printf("can not find member");
			exit(0);
		}
		if (is_func_obj(v_field)) {
			MethodObject *m_o = init_method_object((FunctionObject*) v_field.u.object, i_o);
			v_field.u.object = (MyObject*) m_o;
			save_runtime_stack(frame, v_field);
		} else {
			save_runtime_stack(frame, v_field);/*  后续使用内存拷贝的方式*/
		}
	} else {
		printf("load attribute error");
		exit(0);
	}
}

void vm_store_attr(Frame *frame, Var var) { /*后续可能要加入map类型和class类型*/
	Value v_attr = vector_pop(&frame->runtime);
	Value v_instance = vector_pop(&frame->runtime);
	InstanceObject *i_o = value2instance(v_instance);
	Field field;
	int i;
	for (i = i_o->fields.n - 1; i >= 0; i--) {
		field = i_o->fields.data[i];
		if (field.var == var) {
			(i_o->fields.data[i]).value = v_attr; /*如果已经存在则更改*/
			return;
		}
	}
	field.var = var;
	field.value = v_attr;
	save_field(i_o, field);
}
void vm_build_map(Frame *frame, Instruction code) {
	int n = (code.v < 12) ? 12 : code.v;
	MapObject *o = init_map_object(n);
	int i;
	for (i = 0; i < code.v; i++) {
		Value v1 = vector_pop(&frame->runtime);
		Value v2 = vector_pop(&frame->runtime);
		StringObject *str = value2string(v2);
		int hash_value = util_key2index(str->str_char, n);
		save_hash_node(o->hash_table, v1, str, hash_value);
	}
	save_runtime_stack(frame, obj2value(o));
}
void vm_build_object(Frame *frame, Instruction code) {
	int pos = frame->runtime.n - code.v - 1;
	ClassObject *c_o = value2class(frame->runtime.data[pos]);
	InstanceObject *i_o = init_instance_object(c_o);
	Value rtn_v;
	if (get_class_field(c_o, 1, &rtn_v) == 1) {/*查找初始化constructor的位置*/
		FunctionObject *f_o = value2func(rtn_v);
		if (code.v != f_o->args_num) {
			printf("function call args error2");
			exit(0);
		}
		Frame curr_frame = init_frame(f_o->var.n + 1);
		curr_frame.value[0] = obj2value(i_o);
		int i;
		for (i = 0; i < code.v; i++) {
			curr_frame.value[i + 1] = (frame->runtime.data)[frame->runtime.n - code.v + i];
		}
		save_frame(curr_frame);
		vm((MyParserObject*) f_o);
		Frame *top_frame = pop_frame();
		free_frame(top_frame);
		frame->runtime.n = pos;
	}
	save_runtime_stack(frame, obj2value(i_o));
}

void vm_super(Frame *frame) {
	InstanceObject *this = value2instance(frame->value[0]);
	Value v = obj2value(this->in_class->super);/*这里使用super加入到运行时栈，后续可能会引发bug*/
	save_runtime_stack(frame, v);
}

Value vm(MyParserObject *p) {
	Frame *frame = top_frame();
	Instruction *codes = p->in_code.data;
	Value rtn; /*这里接受返回值信息*/
	rtn.type = NONE;
	int i;
	for (i = 0; i < p->in_code.n; i++) {
		Instruction code = codes[i];
		switch (code.op) {
		case OP_PUSH: {
			save_runtime_stack(frame, p->value.data[code.v]);
			break;
		}
		case OP_LOAD: {
			save_runtime_stack(frame, frame->value[code.v]);
			break;
		}
		case OP_LOAD_GLOBAL: {
			Frame *g_frame = buttom_frame(); /*获得最底层的frame栈就是全局*/
			save_runtime_stack(frame, g_frame->value[code.v]);
			break;
		}
		case OP_STORE: {
			frame->value[code.v] = vector_pop(&frame->runtime);
			break;
		}
		case OP_BUILD_CLASS: {
			vm_build_class(frame, code);
			break;
		}
		case OP_BUILD_OBJECT: {
			vm_build_object(frame, code);
			break;
		}
		case OP_BUILD_MAP: {
			vm_build_map(frame, code);
			break;
		}
		case OP_PLUS_SELF: {
			Value *p_int = &(frame->value[code.v]);
			assert(p_int->type == INTEGER);
			p_int->u.integer++;
			break;
		}
		case OP_FUNC_CALL: {
			vm_func_call(frame, code);
			break;
		}
		case OP_RETURN: {
			rtn = vector_last(&frame->runtime);
			return rtn;
		}
		case OP_LOAD_ATTR: {
			vm_load_attr(frame, code.v);
			break;
		}
		case OP_STORE_ATTR: {
			vm_store_attr(frame, code.v);
			break;
		}
		case OP_THIS: {
			save_runtime_stack(frame, frame->value[0]);
			break;
		}
		case OP_SUPER: {
			vm_super(frame);
			break;
		}
		case OP_NOT:
			break;
		case OP_MINUS:
			break;
		case OP_PLUS:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
		case OP_EQ:
		case OP_NOT_EQ:
		case OP_OR:
		case OP_AND:
		case OP_LT_EQ:
		case OP_GT_EQ:
		case OP_NE:
		case OP_GT:
		case OP_LT: {
			vm_binop_eval(frame, code);
			break;
		}
		case OP_GOTO_IF_FALSE: {
			Value v1 = vector_pop(&frame->runtime);
			switch (v1.type) {
			case FALSE:
				i = code.v - 1;/*这里可能会有bug，后续可以考虑单独使用pc指向所在位置*/
				break;
			case TRUE:
				break;
			default: {
				printf("type is error");
				exit(0);
			}
			}
			break;
		}
		case OP_GOTO: {
			i = code.v - 1; /*因为i在循环结束会增加1，这里可能会有bug，后续可以考虑单独使用pc*/
			break;
		}
		case OP_INDEX: {
			vm_index(frame);
			break;
		}
		case OP_BUILD_LIST: {
			vm_build_list(frame, code);
			break;
		}
		case OP_STORE_INDEX: {
			vm_store_index(frame, code);
			break;
		}
		case OP_ARRAY_LENGTH: {
			vm_array_length(frame);
			break;
		}
		case OP_CONST_INT: {
			Value v_const;
			v_const.type = INTEGER;
			v_const.u.integer = code.v;
			save_runtime_stack(frame, v_const);
			break;
		}
		case OP_CONST_NULL: {
			Value v_null;
			v_null.type = NONE;
			save_runtime_stack(frame, v_null);
			break;
		}
		default:
			printf("error   ,");
			break;
		};
	}
	return rtn;
}

void print_value(Value v) {
	switch (v.type) {
	case NUMBER:
		printf("%f", v.u.number);
		break;
	case INTEGER:
		printf("%d", v.u.integer);
		break;
	case TRUE:
		printf("TRUE");
		break;
	case FALSE:
		printf("FALSE");
		break;
	case NONE:
		printf("NONE");
		break;
	case OBJECT: {
		MyObject *o = v.u.object;
		if (o->in_class == &G_string_class) {
			printf("%s", ((StringObject*) o)->str_char);
		} else if (o->in_class == &G_list_class) {
			VectorValue l_obj = ((ListObject*) o)->list;
			printf("[");
			int i;
			for (i = 0; i < l_obj.n; i++) {
				if (i == l_obj.n - 1) {
					print_value(l_obj.data[i]);
				} else {
					print_value(l_obj.data[i]);
					printf(",");
				}
			}
			printf("]");
		} else if (o->in_class == &G_map_class) {
			MapObject *m_obj = (MapObject*) o;
			printf("{");
			int i;
			HashNode *tmp;
			for (i = 0; i < m_obj->capacity; i++) {
				tmp = m_obj->hash_table[i];
				if (tmp != NULL) {
					printf("%s:", tmp->string->str_char);
					print_value(tmp->v);
					tmp = tmp->next;
					while (tmp != NULL) {
						printf(",");
						printf("%s:", tmp->string->str_char);
						print_value(tmp->v);
						tmp = tmp->next;
					}
					break;
				}
			}
			for (i = i + 1; i < m_obj->capacity; i++) {
				for (tmp = m_obj->hash_table[i]; tmp != NULL; tmp = tmp->next) {
					printf(",");
					printf("%s:", tmp->string->str_char);
					print_value(tmp->v);
				}
			}
			printf("}");
		} else {
			printf("type erorr");
			exit(0);
		}
		break;
	}
	default: {
		printf("type erorr");
		exit(0);
	}
	}
}

void builtin_print_func(int args_num, Value *args) {
	Value v = args[0];
	print_value(v);
	printf("\n");
}

void register_builtin_func(MyParserObject *p, char *name, int args_num, void *f) {
	int pos = save_const_string(name);
	save_incode(p, OP_PUSH, p->value.n);
	BuiltinObject *b = (BuiltinObject*) gc_malloc(sizeof(BuiltinObject));
	b->in_class = &G_builtin_class;
	b->args_n = args_num;
	b->in_func = f;
	Value v = obj2value(b);
	save_value(p, v);
	save_incode(p, OP_STORE, save_loc_var(p, pos));
}
void global_register_func(MyParserObject *p) {
	register_builtin_func(p, "print", 1, builtin_print_func);
}
Lexer* init_lexer(char *content) {
	Lexer *lex = (Lexer*) malloc(sizeof(Lexer));
	vector_new(&lex->buff, char);
	lex->type = TOKEN_EOF;
	lex->curr_line = 0;
	lex->content = content;
	lex->pos = 0;
	lex->curr_char = content[lex->pos];
	return lex;
}
void free_lexer() {
	Lexer *lex = G_info.lexer;
	free(lex->buff.data);
	free(lex->content);
	free(lex);
	G_info.lexer = NULL;
}

char* read_file(char *file_name) {
	FILE *fp = fopen(file_name, "r");
	if (fp == NULL) {
		printf("file %s open failed", file_name);
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	if (file_size == 0) {
		printf("file %s is empty", file_name);
		exit(0);
	};
	rewind(fp);
	char *content = (char*) malloc(file_size + 1);
	memset(content, 0, file_size + 1);
	fread(content, sizeof(char), file_size, fp);
	fclose(fp);
	return content;
}
void init_global() {
	G_info.top_frame = 0;
	G_info.gc_head = NULL;
	G_info.gc_capacity = 128;
	G_info.gc_count = 0;
	G_info.top_parser = NULL;
	vector_init(&G_str_t, StringObject*, 128);
	char *inside_str[NUM_INSIDE] = { "null", "constructor", "this", "string", "list", "map", "function", "method",
			"class", "module", "instance", "builtin" };
	int i;
	for (i = 0; i < NUM_INSIDE; i++) {
		save_const_string(inside_str[i]);
	}
}

void init_parser() {
	char *file_name = "demo.js";
	char *content = read_file(file_name);
	Lexer *lex = init_lexer(content);
	G_info.lexer = lex;
	G_info.top_parser = (MyParserObject*) init_module_object(0);
	global_register_func(G_info.top_parser);
	parser_main(G_info.top_parser, lex);
}
void leave() {
	MyObject *tmp, *curr;
	curr = tmp = G_info.gc_head;
	while (curr != NULL) {
		tmp = curr;
		curr = tmp->next;
		free(tmp);
	}
	free_lexer();
}
void start_compile() {
	Frame frame = init_frame(G_info.top_parser->var.n);
	save_frame(frame);
	vm(G_info.top_parser);
	Frame *top_frame = pop_frame();
	free_frame(top_frame);/*清空frame的数据*/
}

int main() {
	init_global();
	init_parser();
	start_compile();
	leave();
	return 0;
}
