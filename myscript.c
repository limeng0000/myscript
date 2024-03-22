#include "assert.h"
#include "ctype.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

/*#define DEBUG*/
#define MAX_STACK 36864 /*1024*36*/
#define GLOBAL_VARS 512
#define INIT_GC_CAP 1024
#define INIT_INCODE 64
#define INIT_VAR 32
#define INIT_CONST 32
#define INIT_PARSER_TAB 128
#define RESERVE_MAP_CAP 12
#define OP_VALUE (inData[++ip])

#define vec_struct(T, vector) \
    typedef struct            \
    {                         \
        T *data;              \
        int n;                \
        int cap;              \
    } vector

#define expand_vector(this, T)                                               \
    do                                                                       \
    {                                                                        \
        if ((this)->n + 1 > (this)->cap)                                     \
        {                                                                    \
            int capacity = ((this)->cap + 1) * 2;                            \
            (this)->data = (T *)realloc((this)->data, capacity * sizeof(T)); \
            if ((this)->data == NULL)                                        \
            {                                                                \
                printf("memory error");                                      \
                exit(1);                                                     \
            }                                                                \
            (this)->cap = capacity;                                          \
        }                                                                    \
    } while (0)

#define init_vector(this, T, size)                         \
    do                                                     \
    {                                                      \
        (this)->n = 0;                                     \
        (this)->cap = size;                                \
        (this)->data = (T *)malloc(this->cap * sizeof(T)); \
    } while (0)

#define OBJECT_HEAD  \
    ObjectKind type; \
    int mark;        \
    struct MyObject_ *next;

#define init_object_head(obj) \
    obj->mark = 0;            \
    obj->next = vm.gc.head;   \
    vm.gc.head = (MyObject *)obj

#define next_char() lex.currChar = lex.content[++lex.pos]
#define look_char() lex.content[lex.pos + 1]
#define obj_to_val(obj) ((Value){OBJECT, {.object = (MyObject *)obj}})
#define num_to_val(n) ((Value){NUMBER, {.number = n}})
#define tag_label(label) int label = parser.curr->inCode.n
#define set_label(label) parser.curr->inCode.data[label] = parser.curr->inCode.n

typedef enum ValueKind_
{
    NUMBER,
    OBJECT,
    TRUE,
    FALSE,
    NONE,
    SUPER
} ValueKind;
typedef enum ObjectKind_
{
    STRING_OBJ,
    LIST_OBJ,
    MAP_OBJ,
    INSTANCE_OBJ,
    COMPILER_OBJ,
    CLASS_OBJ,
    MEMBER_OBJ,
    BUILDIN_OBJ
} ObjectKind;
typedef enum TokenKind_
{
    TOKEN_VAR = 257,  /*global*/
    TOKEN_FUNCTION,   /*function*/
    TOKEN_IF,         /*if*/
    TOKEN_ELSE,       /*else*/
    TOKEN_TRUE,       /*true*/
    TOKEN_FALSE,      /*false*/
    TOKEN_WHILE,      /*while*/
    TOKEN_FOR,        /*for*/
    TOKEN_BREAK,      /*break*/
    TOKEN_RETURN,     /*return*/
    TOKEN_NULL,       /*NULL*/
    TOKEN_CLASS,      /*class*/
    TOKEN_THIS,       /*this*/
    TOKEN_OF,         /*of*/
    TOKEN_EXTENDS,    /*extends*/
    TOKEN_SUPER,      /*super*/
    TOKEN_NEW,        /*new*/
    TOKEN_IMPORT,     /*import,暂未实现*/
    TOKEN_NUMBER,     /*数字，例如：16.23*/
    TOKEN_STRING,     /*字符串，例如："helloworld"*/
    TOKEN_OR,         /*||*/
    TOKEN_AND,        /*&&*/
    TOKEN_LT_EQ,      /*<=*/
    TOKEN_GT_EQ,      /*>=*/
    TOKEN_EQ,         /*==*/
    TOKEN_NOT_EQ,     /*!=*/
    TOKEN_IDENTIFIER, /*变量，例如：i*/
    TOKEN_EOF         /*end*/
} TokenKind;
typedef enum OpKind_
{
    OP_LOAD_GLOBAL = 0, /*加载全局变量到运行时栈，操作值：字符串在常量数组的位置，执行后：运行时栈+1*/
    OP_BUILD_LIST,      /*创建一个list对象并加载到运行时栈，操作值：list对象的元素数量n，执行后：运行时栈-n+1*/
    OP_BUILD_MAP,       /*创建一个map对象并加载到运行时栈，操作值：map对象的元素数量n*2，执行后：运行时栈-n*2+1*/
    OP_ARRAY_LENGTH,    /*弹出栈顶的list对象获得list长度并加载到运行时栈，操作值：无操作值，执行后:运行时栈+1*/
    OP_LOAD,            /*加载变量到运行时栈，操作值：函数中变量的位置，执行后：运行时栈+1*/
    OP_PUSH,            /*加载常量到运行时栈，操作值：常量所在的位置，执行后：运行时栈+1*/
    OP_SUPER,           /*加载实例对象的父类到运行时栈，操作值：无，执行后：运行时栈+1*/
    OP_THIS,            /*加载实例对象到运行时栈，操作值：无，执行后：运行时栈+1*/
    OP_CONST_INT,       /*加载int数值运行时栈，操作值：数值，执行后：运行时栈+1*/
    OP_CONST_NULL,      /*加载空值到运行时栈，操作值：无，执行后：运行时栈+1*/
    OP_CONST_FALSE,     /*加载假值到运行时栈，操作值：无，执行后：运行时栈+1*/
    OP_CONST_TRUE,      /*加载真值到运行时栈，操作值：无，执行后：运行时栈+1*/
    OP_STORE_GLOBAL,    /*弹出运行时栈顶数据储存到全局变量，操作值：字符串在常量数组的位置，执行后:运行时栈-1*/
    OP_BUILD_CLASS,     /*创建一个Class对象并储存到全局变量，操作值：字符串在常量数组的位置，执行后：运行时栈-n-1*/
    OP_BUILD_OBJECT,    /*根据栈顶的class对象创建实例对象并加载到运行时栈，操作值：初始化方法需要的参数n，执行后：运行时栈-n*/
    OP_FUNC_CALL,       /*根据栈顶的MyCompiler对象调用函数，操作值：函数需要的参数n，执行后：运行时栈-n*/
    OP_METHOD_CALL,     /*根据栈顶-1位置的实例对象等对象调用对象的方法，操作值：方法需要的参数n，执行后：运行时栈-n-1*/
    OP_STORE_ATTR,      /*根据栈顶-1位置的实例对象或者map对象，把栈顶的值写入，操作值：字符串在常量数组的位置，执行后：运行时栈-2*/
    OP_LOAD_ATTR,       /*根据栈顶的实例对象等及字符串key获得value值并写入栈顶，操作值：字符串在常量数组的位置，执行后：不变*/
    OP_RETURN,          /*把栈顶的值写入函数调起时的栈顶的位置，操作值：无，执行后：函数调起时栈顶的位置*/
    OP_INDEX,           /*根据栈顶的索引及栈顶-1位置的list对象或者map对象获取值写入运行时栈,操作值：无，执行后：-1*/
    OP_STORE,           /*弹出栈顶的值写入变量，操作值：函数中变量的位置，执行后：运行时栈-1*/
    OP_STORE_INDEX,     /*根据栈顶的值及栈顶-1位置索引及栈顶-2位置的list对象或者map对象，写入数据,操作值：无，执行后：-3*/
    OP_GOTO,            /*ip跳转到操作值的位置，操作值：代码执行位置，执行后：不变*/
    OP_GOTO_IF_FALSE,   /*根据栈顶的值判断是否是假，如果假跳转到操作值，操作值：代码执行位置，执行后：-1*/
    OP_PLUS_SELF,       /*对变量进行自加操作，操作值：函数中变量的位置，执行后：不变*/
    OP_NOT,             /*弹出栈顶的值并对值取非操作，并写入栈顶，操作值：无，执行后：不变*/
    OP_MINUS,           /*弹出栈顶的值并对值取负操作，并写入栈顶，操作值：无，执行后：不变*/
    OP_PLUS,            /*弹出栈顶的值及栈顶-1的值，相加后写入栈顶，操作值：无，执行后：-1*/
    OP_SUB,             /*弹出栈顶的值及栈顶-1的值，相减后写入栈顶，操作值：无，执行后：-1*/
    OP_MUL,             /*弹出栈顶的值及栈顶-1的值，相乘后写入栈顶，操作值：无，执行后：-1*/
    OP_DIV,             /*弹出栈顶的值及栈顶-1的值，相除后写入栈顶，操作值：无，执行后：-1*/
    OP_EQ,              /*弹出栈顶的值及栈顶-1的值，判断是否相等写入栈顶，操作值：无，执行后：-1*/
    OP_NOT_EQ,          /*弹出栈顶的值及栈顶-1的值，判断是否不相等写入栈顶，操作值：无，执行后：-1*/
    OP_OR,              /*弹出栈顶的值及栈顶-1的值，取或操作写入栈顶，操作值：无，执行后：-1*/
    OP_AND,             /*弹出栈顶的值及栈顶-1的值，取与操作写入栈顶，操作值：无，执行后：-1*/
    OP_LT_EQ,           /*弹出栈顶的值及栈顶-1的值，判断是否小于等于写入栈顶，操作值：无，执行后：-1*/
    OP_GT_EQ,           /*弹出栈顶的值及栈顶-1的值，判断是否大于等于写入栈顶，操作值：无，执行后：-1*/
    OP_LT,              /*弹出栈顶的值及栈顶-1的值，判断是否小于写入栈顶，操作值：无，执行后：-1*/
    OP_GT               /*弹出栈顶的值及栈顶-1的值，判断是否大于写入栈顶，操作值：无，执行后：-1*/
} OpKind;
/*
 * struct MyObject - 对象的结构体，所有对象可以通过强转成MyObject类型获得共有的OBJECT_HEAD这部分数据
 * @OBJECT_HEAD:所有对象共有部分数据
 */
typedef struct MyObject_
{
    OBJECT_HEAD
} MyObject;
/*
 * struct Value - 值的结构体，所有值类型可以通过type识别
 * @type:区分值的类型
 * @u.number:数字值
 * @u.object:对象值
 */
typedef struct Value_
{
    ValueKind type;
    union
    {
        double number;
        MyObject *object;
    } u;
} Value;
/*
 * struct StringObject - 字符串对象
 * @OBJECT_HEAD:对象的头信息
 * @str:字符串
 * @len:字符串长度
 * @hash:字符串的哈希值
 */
typedef struct StringObject_
{
    OBJECT_HEAD
    char *str;
    int len;
    unsigned int hash;
} StringObject;

vec_struct(void *, VecVoid);
vec_struct(Value, VecValue);
vec_struct(int, VecInt);
/*
 * pushIntVec(VecInt *this, int v) - 对int数组插入数据
 * @this:int数组
 * @v:值
 *
 * 判断数组的数量是否超过其容量，如果是则使容量扩展乘其容量+1后乘2，
 * 容量扩展时+1是为了防止原来容量是0的情况，最后在数组末尾插入数据
 *
 * Return:void
 */
void pushIntVec(VecInt *this, int v)
{
    expand_vector(this, int);
    (this->data)[this->n++] = v;
}
void pushValueVec(VecValue *this, Value v)
{
    expand_vector(this, Value);
    (this->data)[this->n++] = v;
}
void pushVoidVec(VecVoid *this, void *s)
{
    expand_vector(this, void *);
    (this->data)[this->n++] = s;
}
/*
 * popIntVec(VecInt *this) - 对int数组弹出数据
 * @this:int数组
 *
 * 判断数组的数量是否大于0，如果小于等于0报错，并返回数组最后的元素
 *
 * Return:int
 */
int popIntVec(VecInt *this)
{
    assert(this->n > 0);
    this->n--;
    return this->data[this->n];
}
Value popValueVec(VecValue *this)
{
    assert(this->n > 0);
    this->n--;
    return this->data[this->n];
}
void *popVoidVec(VecVoid *this)
{
    assert(this->n > 0);
    this->n--;
    return this->data[this->n];
}
/*
 * initIntVec(VecInt *this, int size) - 对int数组进行初始化
 * @this:int数组
 * @size:数组容量大小
 *
 * 根据数组容量大小对数组进行初始化
 *
 * Return:void
 */
void initIntVec(VecInt *this, int size)
{
    init_vector(this, int, size);
}
void initValueVec(VecValue *this, int size)
{
    init_vector(this, Value, size);
}
void initVoidVec(VecVoid *this, int size)
{
    init_vector(this, void *, size);
}
/*
 * util_key2hash() - 根据字符串生成哈希值
 * @key:输入的字符串
 * @len:字符串长度
 *
 * 函数用来计算字符串的哈希值，用来最大化分散数组的字符串，避免字符串的碰撞，算法是：FNV-1a
 *
 * Return:哈希值
 */
unsigned int utilKey2Hash(char *key, int len)
{
    unsigned int hash = 2166136261u;
    int i;
    for (i = 0; i < len; i++)
    {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}
/*
 * utilStrdup(const char *s) - 根据字符串拷贝函数
 * @s:输入的字符串
 *
 * 用于将c语言中引号括起来的字符串，拷贝到内存中方便内存管理
 *
 * Return:字符串
 */
char *utilStrdup(const char *s)
{
    int size = strlen(s) + 1;
    char *p = malloc(size);
    if (p)
    {
        memcpy(p, s, size);
    }
    return p;
}

/*************************
 * 以下词法分析器部分
 *************************
 */
/*
 * struct Token - 标识符结构体，经过词法分析后用来识别字符表示的含义
 * @type:类型，对应枚举TokenKind及acsii小于257的单字符
 * @line：所在的行，方便后续虚拟机打印报错信息
 * @start：token在程序文本中的开始位置
 * @u.number:数字值
 * @u.str:字符串值
 */
typedef struct Token_
{
    int type;
    int line;
    int start;
    union
    {
        double number;
        StringObject *str;
    } u;
} Token;
/*
 * struct StringMap - 用来存放词法分析阶段的哈希map-拉链法
 * @cap：哈希map的容量
 * @tabStr：数量为cap的哈希map
 */
typedef struct StringMap_
{
    int cap;
    StringObject **tabStr;
} StringMap;
/*
 * struct Lexer - 词法分析器结构体
 * @token：当前标识符
 * @content：程序文本字符串
 * @size：程序文本大小
 * @pos：当前字符在程序文本的位置
 * @currChar：当前字符
 * @strMap：字符串的哈希map
 * @keysWords：myscript语言的关键字
 */
typedef struct Lexer_
{
    Token token;
    char *content;
    int size;
    int pos;
    char currChar;
    StringMap strMap;
    unsigned int keysWords[18];
} Lexer;

Lexer lex; /*全局词法分析器*/

/*
 * syntaxError(char *msg) - 打印语法分析错误并退出
 * @msg:需要输出的错误字符
 * Return:void
 */
void syntaxError(char *msg)
{
    printf("syntaxError:line [%d:%d] >>>>>> %s", lex.token.line, lex.token.start, msg);
    exit(1);
}
/*
 * initStringMap(StringMap *obj, int size) - 初始化词法分析阶段哈希map
 * @obj:哈希map
 * @size:哈希map容量
 * Return:void
 */
void initStringMap(StringMap *obj, int size)
{
    obj->cap = size;
    obj->tabStr = (StringObject **)malloc(sizeof(StringObject *) * size);
    int i;
    for (i = 0; i < size; i++)
    {
        obj->tabStr[i] = NULL;
    }
}
/*
 * readFile(const char *file) - 读取程序文本文件
 * @file:程序文本文件名
 *
 * 读取文件内容到内存中并将内容返回
 *
 * Return:程序文本字符串
 */
char *readFile(const char *file)
{
    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        printf("file %s open failed", file);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    if (size == 0)
    {
        printf("file %s is empty", file);
        exit(1);
    };
    rewind(fp);
    char *content = (char *)malloc(size + 1);
    memset(content, 0, size + 1);
    fread(content, sizeof(char), size, fp);
    fclose(fp);
    return content;
}
StringObject *tokenSaveStr(char *from, int len, unsigned int hash);
/*
 * StringObject *initLexer(const char *file) - 初始化词法分析器
 * @file:程序文本文件名
 *
 * 通过全局变量lex初始化词法分析器的各个成员变量，并把程序文件名包装成StringObject类型返回
 * 这里strMap容量的大小等于程序文本大小除5加10作为初始容量，且不会改变
 *
 * Return:StringObject类型的程序文本文件名
 */
StringObject *initLexer(const char *file)
{
    char *content = readFile(file);
    lex.token.type = TOKEN_EOF;
    lex.token.line = 0;
    lex.token.start = 0;
    lex.content = content;
    lex.size = strlen(content);
    lex.pos = 0;
    lex.currChar = lex.content[lex.pos];
    initStringMap(&lex.strMap, (int)(lex.size / 5) + 10);
    lex.keysWords[0] = utilKey2Hash("var", 3);
    lex.keysWords[1] = utilKey2Hash("function", 8);
    lex.keysWords[2] = utilKey2Hash("if", 2);
    lex.keysWords[3] = utilKey2Hash("else", 4);
    lex.keysWords[4] = utilKey2Hash("true", 4);
    lex.keysWords[5] = utilKey2Hash("false", 5);
    lex.keysWords[6] = utilKey2Hash("while", 5);
    lex.keysWords[7] = utilKey2Hash("for", 3);
    lex.keysWords[8] = utilKey2Hash("break", 5);
    lex.keysWords[9] = utilKey2Hash("return", 6);
    lex.keysWords[10] = utilKey2Hash("null", 4);
    lex.keysWords[11] = utilKey2Hash("class", 5);
    lex.keysWords[12] = utilKey2Hash("this", 4);
    lex.keysWords[13] = utilKey2Hash("of", 2);
    lex.keysWords[14] = utilKey2Hash("extends", 7);
    lex.keysWords[15] = utilKey2Hash("super", 5);
    lex.keysWords[16] = utilKey2Hash("new", 3);
    lex.keysWords[17] = utilKey2Hash("import", 6);
    char *filename = utilStrdup(file);
    int len = strlen(filename);
    return tokenSaveStr(filename, len, utilKey2Hash(filename, len));
}
/*
 * eatToken(int token_enum) - 保存当前token类型，当前字符指向下一个
 * @token_enum:token类型
 *
 * 用于单字符的识别比如赋值操作'=',把该的acsii的值作为token_enum保存到lex中,当前字符指向下一个
 *
 * Return:void
 */
void eatToken(int token_enum)
{
    lex.token.type = token_enum;
    next_char();
}
/*
 * eat2Token(int token_enum) - 保存当前token类型，当前字符指向下下一个
 * @token_enum:token类型
 *
 * 用于两个字符的识别比如判断是否相等'==',把字符对应的枚举类型TokenKind转成int类型保存到lex中,当前字符指向下下一个
 *
 * Return:void
 */
void eat2Token(int token_enum)
{
    lex.token.type = token_enum;
    lex.pos++;
    next_char();
}
/*
 * initStringNoGc(char *str) - 初始化不会被GC回收的字符串对象
 * @str:字符串
 *
 * 给StringObject对象分配内存，初始化各个成员变量
 * 用这个函数创建的StringObject对象next指向NULL且没有加入GC，不会被GC回收
 *
 * Return:StringObject类型的字符串
 */
StringObject *initStringNoGc(char *str)
{
    StringObject *obj = (StringObject *)malloc(sizeof(StringObject));
    obj->type = STRING_OBJ;
    obj->len = strlen(str);
    obj->hash = utilKey2Hash(str, obj->len);
    obj->str = str;
    obj->next = NULL;
    obj->mark = 0;
    return obj;
}
/*
 * tokenSaveStr(char *from, int len, unsigned int hash) - 保存字符串词法分析阶段的哈希map中
 * @from:字符串
 * @len:字符串长度
 * @hash:字符串哈希值
 *
 * 对输入的字符串判断时候已经在词法分析阶段的哈希map中，如果存在直接返回哈希map中的StringObject对象
 * 如果不存在分配内存并复制from中的字符到内存中，并使用initStringNoGc方法创建StringObject对象，
 * 并把该对象加入到词法分析阶段的哈希map中
 *
 * Return:StringObject类型的字符串
 */
StringObject *tokenSaveStr(char *from, int len, unsigned int hash)
{
    int pos = hash % lex.strMap.cap;
    StringObject *tmp;
    for (tmp = lex.strMap.tabStr[pos]; tmp != NULL; tmp = (StringObject *)tmp->next)
    {
        if (tmp->hash == hash)
        {
            return tmp;
        }
    }
    char *toStr = (char *)malloc(len + 1);
    memcpy(toStr, from, len);
    toStr[len] = '\0';
    StringObject *str = initStringNoGc(toStr);
    str->mark = 0;
    str->next = (MyObject *)lex.strMap.tabStr[pos];
    lex.strMap.tabStr[pos] = str;
    return str;
}
/*
 * tokenString() - 保存字符串到token中
 *
 * 循环判断当前字符是否是'"'，如果不是继续循环，如果是说明已经把字符串识别出来
 * 把识别出来的字符串保存到token中
 *
 * Return:void
 */
void tokenString()
{
    next_char();
    while (lex.currChar != '"')
    {
        next_char();
    }
    next_char();
    char *from = lex.content + lex.token.start + 1;
    int len = lex.pos - lex.token.start - 2;
    int hash = utilKey2Hash(from, len);
    lex.token.u.str = tokenSaveStr(from, len, hash);
    lex.token.type = TOKEN_STRING;
}
/*
 * tokenNumber() - 保存数字到token中
 *
 * 循环判断当前字符是否是数字，如果不是退出循环，然后判断数字是否带有小数点
 * 把识别出来的数字保存到token中
 *
 * Return:void
 */
void tokenNumber()
{
    while (isdigit(lex.currChar))
    {
        next_char();
    }
    if (lex.currChar == '.')
    {
        next_char();
        if (!isdigit(lex.currChar))
        {
            syntaxError("number token error");
        }
        while (isdigit(lex.currChar))
        {
            next_char();
        }
    }
    double num = strtod(lex.content + lex.token.start, NULL);
    lex.token.u.number = num;
    lex.token.type = TOKEN_NUMBER;
}
/*
 * tokenIdentifier() - 保存关键字或变量到token中
 *
 * 循环判断当前字符是否是字符或者_，如果不是退出循环，退出循环时的位置减开始的位置就是字符的全部内容，
 * 计算字符的hash值与关键字的hash值对比，对比成功说明是关键字把token类型设置成对应的TokenKind并返回，
 * 如果没有对比成功说明是变量，把变量的字符串保存到token，并把token类型设置成TOKEN_IDENTIFIER
 *
 * Return:void
 */
void tokenIdentifier()
{
    if (isalpha(lex.currChar) || lex.currChar == '_')
    {
        next_char();
    }
    while (isalpha(lex.currChar) || lex.currChar == '_' || isdigit(lex.currChar))
    {
        next_char();
    }
    char *from = lex.content + lex.token.start;
    int len = lex.pos - lex.token.start;
    unsigned int hash = utilKey2Hash(from, len);
    int i;
    for (i = 0; i < 18; i++)
    {
        if (lex.keysWords[i] == hash)
        {
            lex.token.type = i + 257;
            return;
        }
    }
    lex.token.u.str = tokenSaveStr(from, len, hash);
    lex.token.type = TOKEN_IDENTIFIER;
}
/*
 * skipComment() - 跳过注释
 *
 * 对当前位置加2跳过//字符,循环判断字符串是否是换行，如果是换行跳出循环
 *
 * Return:void
 */
void skipComment()
{
    lex.pos += 2;
    lex.currChar = lex.content[lex.pos];
    while (lex.currChar != '\n')
    {
        next_char();
    }
}
/*
 * skipBlank() - 跳过换行、注释、空白
 *
 * 循环判断当前字符时候是换行、注释、空白，如果是忽略内容
 *
 * Return:void
 */
void skipBlank()
{
    while (1)
    {
        switch (lex.currChar)
        {
        case '\n':
        {
            next_char();
            lex.token.line++;
            break;
        }
        case '/':
        {
            if (look_char() == '/')
            {
                skipComment();
                break;
            }
            else
            {
                return;
            }
        }
        case ' ':
        case '\t':
        case '\v':
            next_char();
            break;
        default:
            return;
        }
    }
}
/*
 * nextToken() - 调用函数会产生下一个token
 *
 * 该方法是词法分析器的入口，语法分析器通过调用该函数会产生下一个TOKEN，通过TOKEN信息来确定适配对应的语法范式
 * 在开始获得下一个token之前调用skipBlank函数跳过换行、注释、空白等内容，然后循环判断当前字符属于那些标识符
 * 并把标识符信息保存在token中
 *
 * Return:void
 */
void nextToken()
{
    skipBlank();
    lex.token.type = TOKEN_EOF;
    lex.token.start = lex.pos;
    switch (lex.currChar)
    {
    case '|':
    {
        if (look_char() == '|')
        {
            eat2Token(TOKEN_OR);
        }
        else
        {
            eatToken('|');
        }
        break;
    }
    case '&':
    {
        if (look_char() == '&')
        {
            eat2Token(TOKEN_AND);
        }
        else
        {
            eatToken('&');
        }
        break;
    }
    case '<':
    {
        if (look_char() == '=')
        {
            eat2Token(TOKEN_LT_EQ);
        }
        else
        {
            eatToken('<');
        }
        break;
    }
    case '>':
    {
        if (look_char() == '=')
        {
            eat2Token(TOKEN_GT_EQ);
        }
        else
        {
            eatToken('>');
        }
        break;
    }
    case '=':
    {
        if (look_char() == '=')
        {
            eat2Token(TOKEN_EQ);
        }
        else
        {
            eatToken('=');
        }
        break;
    }
    case '!':
    {
        if (look_char() == '=')
        {
            eat2Token(TOKEN_NOT_EQ);
        }
        else
        {
            eatToken('!');
        }
        break;
    }
    case '\0':
    {
        eatToken(TOKEN_EOF);
        break;
    }
    case '"':
    {
        tokenString();
        break;
    }
    default:
    {
        if (isdigit(lex.currChar))
        {
            tokenNumber();
        }
        else if (isalpha(lex.currChar) || lex.currChar == '_')
        {
            tokenIdentifier();
        }
        else
        {
            eatToken(lex.currChar);
        }
        break;
    }
    }
}
/*********************
 * 以下语法分析器部分
 *********************
 */
/*
 * struct MyCompiler - myscript函数编译环境结构体
 * @OBJECT_HEAD：对象头
 * @inCode：字节码数组
 * @lines：所在行数组
 * @consts：常量数组
 * @var：变量数组
 * @name：函数名
 * @argsNum：函数参数
 * @maxOccupyStack：函数调起时将占用多少运行时栈
 */
typedef struct MyCompiler_
{
    OBJECT_HEAD
    VecInt inCode;
    VecInt lines;
    VecValue consts;
    VecVoid var;
    StringObject *name;
    int argsNum;
    int maxOccupyStack;
} MyCompiler;
/*
 * struct parser - 语法分析器结构体
 * @top：最顶层编译环境（即全局编译环境）
 * @curr：当前函数编译环境
 * @head：函数编译环境链表的头节点
 * @tabElse：else数组，goto字节码跳转位置时用到
 * @tabBreak：break数组，goto字节码跳转位置时用到
 * @tabFor：for数组，for循环中末尾循环体字节码信息保存在该数组中
 * @tabLine：line数组，for循环中末尾循环体字节码对应的行信息保存在该数组中
 */
typedef struct Parser_
{
    MyCompiler *top;
    MyCompiler *curr;
    MyCompiler *head;
    VecInt tabElse;
    VecInt tabBreak;
    VecInt tabFor;
    VecInt tabLine;
} Parser;

Parser parser; /*全局语法分析器*/

/*
 * initCompiler(StringObject *name) - 初始化函数编译环境对象
 * @name:编译环境名
 *
 * 给MyCompiler对象分配内存，初始化各个成员变量，并把MyCompiler对象加入到语法分析器的head链表中
 *
 * Return:函数编译环境对象
 */
MyCompiler *initCompiler(StringObject *name)
{
    MyCompiler *c = (MyCompiler *)malloc(sizeof(MyCompiler));
    c->mark = 0;
    c->next = NULL;
    c->type = COMPILER_OBJ;
    initIntVec(&c->inCode, INIT_INCODE);
    initIntVec(&c->lines, INIT_INCODE);
    initValueVec(&c->consts, INIT_CONST);
    initVoidVec(&c->var, INIT_VAR);
    c->name = name;
    c->argsNum = 0;
    c->maxOccupyStack = 0;
    c->next = (MyObject *)parser.head;
    parser.head = c;
    return c;
}
/*
 * initParser(MyCompiler *top) - 初始化语法分析器
 * @top:最顶层的编译环境（即全局编译环境）
 *
 * 给MyCompiler对象分配内存，初始化各个成员变量，并把MyCompiler对象加入到语法分析器的head链表中
 *
 * Return:void
 */
void initParser(MyCompiler *top)
{
    parser.top = top;
    parser.curr = top;
    parser.head = top;
    initIntVec(&parser.tabElse, INIT_PARSER_TAB);
    initIntVec(&parser.tabBreak, INIT_PARSER_TAB);
    initIntVec(&parser.tabFor, INIT_PARSER_TAB);
    initIntVec(&parser.tabLine, INIT_PARSER_TAB);
}
/*
 * saveIncode(int op, int v) - 保存操作码及操作值到当前编译环境的字节码数组中
 * @op:操作码
 * @v:操作值
 *
 * 枚举类型OpKind的字节码转化成int类型，如果数值小于12说明，该操作码会使运行栈的栈顶加1，所以maxOccupyStack自增1
 * 在保存操作码及操作值的同时保存操作码所在的文本行信息
 *
 * Return:void
 */
void saveIncode(int op, int v)
{
    if (op < 12)
    {
        parser.curr->maxOccupyStack++;
    }
    pushIntVec(&parser.curr->inCode, op);
    pushIntVec(&parser.curr->inCode, v);
    pushIntVec(&parser.curr->lines, lex.token.line);
    pushIntVec(&parser.curr->lines, lex.token.line);
}
/*
 * saveIncodeOp(int op) - 只保存操作码到当前编译环境的字节码数组中
 * @op:操作码
 *
 * 有些操作码后面没有操作值，例如:OP_CONST_NULL,所以只用保存操作码
 *
 * Return:void
 */
void saveIncodeOp(int op)
{
    if (op < 12)
    {
        parser.curr->maxOccupyStack++;
    }
    pushIntVec(&parser.curr->inCode, op);
    pushIntVec(&parser.curr->lines, lex.token.line);
}
/*
 * saveConst(int op, Value v) - 保存常量到当前编译环境的常量数组中及操作码到字节码数组中
 * @op:操作码
 * @v:常量
 *
 * 函数保存常量到当前编译环境的常量数组中并把操作码与常量所在常量数组的位置当作操作值一起保存在字节码数组
 *
 * Return:void
 */
void saveConst(int op, Value v)
{
    pushValueVec(&parser.curr->consts, v);
    saveIncode(op, parser.curr->consts.n - 1);
}
/*
 * saveNoneVar() - 保存一个空的变量到当前编译环境的变量数组中
 *
 * 变量会占用运行时栈，在保存变量的时候需要maxOccupyStack自增1
 *
 * Return:变量所在变量数组中的位置
 */
int saveNoneVar()
{
    pushVoidVec(&parser.curr->var, NULL);
    parser.curr->maxOccupyStack++;
    return parser.curr->var.n - 1;
}
/*
 * getLocalVar(StringObject *str) - 在当前编译环境中查找变量
 * @str:变量
 *
 * 在当前编译环境的变量数组中通过倒序的方式查找变量，因为通常情况一个变量赋值完就会使用，所以先查找最后保存的，最后查找最先保存的
 * 如果查询到返回变量所在变量数组中的位置，如果查询不到返回-1
 *
 * Return:变量所在变量数组中的位置
 */
int getLocalVar(StringObject *str)
{
    int i;
    for (i = parser.curr->var.n - 1; i >= 0; i--)
    {
        StringObject *v = parser.curr->var.data[i];
        if (v != NULL && v->hash == str->hash)
        {
            return i;
        }
    }
    return -1;
}
/*
 * saveLocalVar(StringObject *str) - 保存变量到当前编译环境中
 * @str:变量
 *
 * 先查找当前编译环境是否有该变量，如果有返回变量所在变量数组中的位置
 * 如果没有，把变量插入到当前编译环境变量数组的最后的位置，并返回位置的索引
 *
 * Return:变量所在变量数组中的位置
 */
int saveLocalVar(StringObject *str)
{
    int i = getLocalVar(str);
    if (i != -1)
    {
        return i;
    }
    pushVoidVec(&parser.curr->var, str);
    parser.curr->maxOccupyStack++;
    return parser.curr->var.n - 1;
}
/*
 * loadVar(StringObject *str)  - 根据变量及环境确定加载变量时使用何种操作码
 * @str:变量
 *
 * 当编译时遇到一个变量，如果当前编译环境不是全局编译环境（在函数中）且在当前编译环境中能查找到变量
 * 操作码是OP_LOAD，操作值是变量所在当前编译环境变量数组中的位置，
 * 如果当前编译环境是全局编译环境（在全局中）或在当前编译环境中不能查找到变量
 * 操作码是OP_LOAD_GLOBAL，操作值是变量所在当前编译环境常量数组中的位置
 * 函数在编译阶段告诉虚拟机使用那种操作码加载变量
 * OP_LOAD操作码将在运行时栈函数帧中的操作值位置加载变量的值到运行时栈的栈顶
 * OP_LOAD_GLOBAL操作码将将在运行时在全局变量哈希map中加载变量值到运行时栈的栈顶
 *
 * Return:void
 */
void loadVar(StringObject *str)
{
    int pos = getLocalVar(str);
    if (pos != -1 && parser.top != parser.curr)
    {
        saveIncode(OP_LOAD, pos);
    }
    else
    {
        saveConst(OP_LOAD_GLOBAL, obj_to_val(str));
    }
}
/*
 * storeVar(StringObject *str)  - 根据变量及环境确定赋值变量时使用何种操作码
 * @str:变量
 *
 * 当编译时遇到需要赋值一个变量，如果当前编译环境是全局编译环境
 * 操作码是OP_STORE_GLOBAL，操作值是变量所在当前编译环境常量数组中的位置，
 * 如果当前编译环境不是全局编译环境，调用saveLocalVar保存变量
 * 操作码是OP_STORE，操作值是变量所在当前编译环境变量数组中的位置
 *
 * Return:void
 */
void storeVar(StringObject *str)
{
    if (parser.top == parser.curr)
    {
        saveConst(OP_STORE_GLOBAL, obj_to_val(str));
    }
    else
    {
        int pos = saveLocalVar(str);
        saveIncode(OP_STORE, pos);
    }
}
/*
 * checkToken(int tokenType)  - 校验当前token类型是否于传入的token类型一致
 * @tokenType:token类型
 *
 * 如果与传入token类型不一致打印错误信息并退出
 *
 * Return:void
 */
void checkToken(int tokenType)
{
    if (lex.token.type != tokenType)
    {
        syntaxError("token appear here does not match expected");
    }
}
/*
 * checkSkip(int tokenType)  - 校验当前token类型是否于传入的token类型一致并跳过该token
 * @tokenType:token类型
 *
 * 如果校验传入token类型一致，调用nextToken产生下一个token
 *
 * Return:void
 */
void checkSkip(int tokenType)
{
    checkToken(tokenType);
    nextToken();
}
void exprRvalue();
/*
 * exprCommaList()  - 逗号分割表达式
 *
 * 范式:exprCommaList -> exprRvalue {',' exprRvalue}
 *
 * 该表达式的使用场景，例如调用函数时传递参数f(a,b,c,d),这里参数a,b,c,d需要用到该范式
 *
 * Return:逗号分割表达式的数量
 */
int exprCommaList()
{
    int n = 1;
    exprRvalue();
    while (lex.token.type == ',')
    {
        nextToken();
        exprRvalue();
        n++;
    }
    return n;
}
/*
 * exprBracket()  - 括号表达式
 *
 * 范式:exprBracket -> '(' exprCommaList ')'
 *
 * 该表达式的使用场景，例如调用函数时传递参数f(a,b,c,d),这里括号部分需要用到该范式
 *
 * Return:逗号分割表达式的数量
 */
int exprBracket()
{
    int n = 0;
    checkSkip('(');
    if (lex.token.type != ')')
    {
        n = exprCommaList();
    }
    checkSkip(')');
    return n;
}
/*
 * struct ExprType - 表达式类型
 * @flag：表达式标识（0代表无,1是变量，2是成员，3是字符串，4是索引，5是方法调起，6是其他）
 * @str：字符串（flag是1时str是变量，2是str是成员变量）
 */
typedef struct ExprType_
{
    int flag;
    StringObject *str;
} ExprType;
/*
 * exprComplex()  - 复合表达式
 *
 * 范式:exprComplex -> ( IDENTIFIER | STRING |'(' exprRvalue ')'| this | super )
 *                          { '.' IDENTIFIER [ exprBracket ] | '[' exprRvalue ']' | exprBracket }
 *
 * 该表达式的场景，例如 变量，字符串，this，super，()括号包裹的部分，后面可以存在:成员变量object.member,
 * 成员函数的调用:object.toString(),索引的调用list[1],函数的调用f(a,b,c)
 * 该表达式匹配完会判断下一个token是否是赋值语句，如果是赋值语句会返回表达式类型，如果不是赋值语句会生成对应的操作码及操作值写入字节码数组
 *
 * Return:表达式类型
 */
ExprType exprComplex()
{
    ExprType e;
    e.flag = 0;
    e.str = NULL;
    switch (lex.token.type)
    {
    case TOKEN_IDENTIFIER:
    {
        e.flag = 1;
        e.str = lex.token.u.str;
        nextToken();
        if (lex.token.type == '=')
        {
            return e;
        }
        loadVar(e.str);
        break;
    }
    case TOKEN_STRING:
    {
        e.flag = 3;
        e.str = lex.token.u.str;
        nextToken();
        saveConst(OP_PUSH, obj_to_val(e.str));
        break;
    }
    case '(':
    {
        e.flag = 6;
        nextToken();
        exprRvalue();
        checkSkip(')');
        break;
    }
    case TOKEN_THIS:
    {
        e.flag = 6;
        nextToken();
        saveIncodeOp(OP_THIS);
        break;
    }
    case TOKEN_SUPER:
    {
        e.flag = 6;
        nextToken();
        saveIncodeOp(OP_SUPER);
        break;
    }
    default:
        break;
    }
    while (1)
    {
        switch (lex.token.type)
        {
        case '.':
        {
            if (e.flag == 0)
            {
                syntaxError("no calls can appear here");
            }
            nextToken();
            checkToken(TOKEN_IDENTIFIER);
            e.flag = 2;
            e.str = lex.token.u.str;
            nextToken();
            if (lex.token.type == '=')
            {
                return e;
            }
            else if (lex.token.type == '(')
            {
                e.flag = 5;
                int count = exprBracket();
                saveIncode(OP_CONST_INT, count);
                saveConst(OP_METHOD_CALL, obj_to_val(e.str));
            }
            else
            {
                saveConst(OP_LOAD_ATTR, obj_to_val(e.str));
            }
            break;
        }
        case '[':
        {
            if (e.flag == 0 || e.flag == 3 || e.flag == 6)
            {
                syntaxError("index call error");
            }
            e.flag = 4;
            nextToken();
            exprRvalue();
            checkSkip(']');
            if (lex.token.type == '=')
            {
                return e;
            }
            saveIncodeOp(OP_INDEX);
            break;
        }
        case '(':
        {
            if (e.flag == 0 || e.flag == 3 || e.flag == 6)
            {
                syntaxError("function call can not appear here");
            }
            e.flag = 5;
            saveIncode(OP_FUNC_CALL, exprBracket());
            break;
        }
        default:
        {
            return e;
        }
        }
    }
    return e;
}
/*
 * exprBase()  - 基础表达式
 *
 * 范式:exprBase -> NULL | TRUE | FALSE | NUMBER | exprComplex
 *
 * Return:void
 */
void exprBase()
{
    switch (lex.token.type)
    {
    case TOKEN_NULL:
    {
        saveIncodeOp(OP_CONST_NULL);
        nextToken();
        break;
    }
    case TOKEN_TRUE:
    {
        saveIncodeOp(OP_CONST_TRUE);
        nextToken();
        break;
    }
    case TOKEN_FALSE:
    {
        saveIncodeOp(OP_CONST_FALSE);
        nextToken();
        break;
    }
    case TOKEN_NUMBER:
    {
        saveConst(OP_PUSH, num_to_val(lex.token.u.number));
        nextToken();
        break;
    }
    default:
    {
        exprComplex();
        break;
    }
    }
}
/*
 * exprPrefix()  - 前缀表达式
 *
 * 范式:exprPrefix -> [ '!' | '-' exprPrefix ]  | exprBase
 * 如果前面有取非操作符或者取负操作符递归调用exprPrefix，没有调用exprBase
 *
 * Return:void
 */
void exprPrefix()
{
    if (lex.token.type == '!' || lex.token.type == '-')
    {
        int op = lex.token.type;
        nextToken();
        exprPrefix();
        if (op == '!')
        {
            saveIncodeOp(OP_NOT);
        }
        else if (op == '-')
        {
            saveIncodeOp(OP_MINUS);
        }
    }
    else
    {
        exprBase();
    }
}
/*
 * exprMulDiv()  - 乘除表达式
 *
 * 范式:exprMulDiv -> exprPrefix { '*' | '/' exprPrefix }
 *
 * Return:void
 */
void exprMulDiv()
{
    exprPrefix();
    while (lex.token.type == '*' || lex.token.type == '/')
    {
        int op = lex.token.type;
        nextToken();
        exprPrefix();
        if (op == '*')
        {
            saveIncodeOp(OP_MUL);
        }
        else
        {
            saveIncodeOp(OP_DIV);
        }
    }
}
/*
 * exprPlusMinus()  - 加减表达式
 *
 * 范式:exprPlusMinus -> exprMulDiv { '+' | '-' exprMulDiv }
 *
 * Return:void
 */
void exprPlusMinus()
{
    exprMulDiv();
    while (lex.token.type == '+' || lex.token.type == '-')
    {
        int op = lex.token.type;
        nextToken();
        exprMulDiv();
        if (op == '+')
        {
            saveIncodeOp(OP_PLUS);
        }
        else
        {
            saveIncodeOp(OP_SUB);
        }
    }
}
/*
 * exprBoolean()  - 判断大小表达式
 *
 * 范式:exprBoolean -> exprPlusMinus { '>' | '<' | '>=' | '<=' exprPlusMinus }
 *
 * Return:void
 */
void exprBoolean()
{
    exprPlusMinus();
    while (1)
    {
        switch (lex.token.type)
        {
        case '>':
        {
            nextToken();
            exprPlusMinus();
            saveIncodeOp(OP_GT);
            break;
        }
        case '<':
        {
            nextToken();
            exprPlusMinus();
            saveIncodeOp(OP_LT);
            break;
        }
        case TOKEN_LT_EQ:
        {
            nextToken();
            exprPlusMinus();
            saveIncodeOp(OP_LT_EQ);
            break;
        }
        case TOKEN_GT_EQ:
        {
            nextToken();
            exprPlusMinus();
            saveIncodeOp(OP_GT_EQ);
            break;
        }
        default:
            return;
        }
    }
}
/*
 * exprEqual()  - 判断相等表达式
 *
 * 范式:exprEqual -> exprBoolean { '==' | '!='  exprBoolean }
 *
 * Return:void
 */
void exprEqual()
{
    exprBoolean();
    while (lex.token.type == TOKEN_EQ || lex.token.type == TOKEN_NOT_EQ)
    {
        int op = lex.token.type;
        nextToken();
        exprBoolean();
        if (op == TOKEN_EQ)
        {
            saveIncodeOp(OP_EQ);
        }
        else
        {
            saveIncodeOp(OP_NOT_EQ);
        }
    }
}
/*
 * exprLogicalAnd()  - 且表达式
 *
 * 范式:exprLogicalAnd -> exprEqual { '&&'   exprEqual }
 *
 * Return:void
 */
void exprLogicalAnd()
{
    exprEqual();
    while (lex.token.type == TOKEN_AND)
    {
        nextToken();
        exprEqual();
        saveIncodeOp(OP_AND);
    };
}
/*
 * exprLogicalOr()  - 或表达式
 *
 * 范式:exprLogicalOr -> exprLogicalAnd { '||'   exprLogicalAnd }
 *
 * Return:void
 */
void exprLogicalOr()
{
    exprLogicalAnd();
    while (lex.token.type == TOKEN_OR)
    {
        nextToken();
        exprLogicalAnd();
        saveIncodeOp(OP_OR);
    };
}
/*
 * exprMap()  - 哈希map表达式
 *
 * 范式:exprMap -> '{' [ IDENTIFIER ':' exprRvalue {',' IDENTIFIER ':' exprRvalue } ] '}'
 *
 * Return:void
 */
void exprMap()
{
    checkSkip('{');
    int n = 0;
    while (lex.token.type == TOKEN_IDENTIFIER || lex.token.type == TOKEN_STRING)
    {
        saveConst(OP_PUSH, obj_to_val(lex.token.u.str));
        nextToken();
        if (lex.token.type == ':')
        {
            nextToken();
            exprRvalue();
        }
        else
        {
            syntaxError("missing colon error");
        }
        n++;
        if (lex.token.type == ',')
        {
            nextToken();
        }
        else if (lex.token.type == '}')
        {
            break;
        }
        else
        {
            syntaxError("token appear here does not match expected");
        }
    }
    checkSkip('}');
    saveIncode(OP_BUILD_MAP, n);
}
/*
 * exprArray()  - 列表表达式
 *
 * 范式:exprArray -> '[' exprCommaList ']'
 *
 * Return:void
 */
void exprArray()
{
    int n = 0;
    checkSkip('[');
    if (lex.token.type != ']')
    {
        n = exprCommaList();
    }
    checkSkip(']');
    saveIncode(OP_BUILD_LIST, n);
}
/*
 * exprNew()  - 创建实例对象表达式
 *
 * 范式:exprNew -> NEW IDENTIFIER exprBracket
 *
 * Return:void
 */
void exprNew()
{
    saveConst(OP_LOAD_GLOBAL, obj_to_val(lex.token.u.str));
    nextToken();
    int n = exprBracket();
    saveIncode(OP_BUILD_OBJECT, n);
}
void blockState();
/*
 * exprLambda()  - 创建匿名函数表达式
 *
 * 范式:exprLambda -> (' [ IDENTIFIER { ',' IDENTIFIER } ] ')' blockState
 *
 * Return:void
 */
void exprLambda(StringObject *name)
{
    MyCompiler *prev = parser.curr;
    MyCompiler *c = initCompiler(name);
    parser.curr = c;
    saveNoneVar();
    int argsNum = 0;
    checkSkip('(');
    while (lex.token.type == TOKEN_IDENTIFIER)
    {
        saveLocalVar(lex.token.u.str);
        nextToken();
        argsNum++;
        if (lex.token.type == ')')
        {
            break;
        }
        checkSkip(',');
    }
    checkSkip(')');
    blockState();
    saveIncodeOp(OP_CONST_NULL);
    saveIncodeOp(OP_RETURN);
    parser.curr->argsNum = argsNum;
    parser.curr = prev;
    saveConst(OP_PUSH, obj_to_val(c));
}
/*
 * exprRvalue()  - 右值表达式
 *
 * 范式:exprRvalue -> exprMap | exprArray | exprNew | exprLambda | exprLogicalOr
 * 右值表达式是赋值语句等号右边的值
 *
 * Return:void
 */
void exprRvalue()
{
    switch (lex.token.type)
    {
    case '{':
    {
        exprMap();
        break;
    }
    case '[':
    {
        exprArray();
        break;
    }
    case TOKEN_NEW:
    {
        nextToken();
        exprNew();
        break;
    }
    case TOKEN_FUNCTION:
    {
        nextToken();
        exprLambda(NULL);
        break;
    }
    default:
        exprLogicalOr();
        break;
    }
}
/*
 * exprState()  - 表达式语句
 *
 * 范式:exprState -> exprComplex | exprComplex '=' exprRvalue
 * 表达式语句包含表达式所有的操作类型，任何表达式都可以通过该表达式语句找到对应的范式
 * 例如：赋值操作，如：a=2+3*5，单独的函数调用，如：a() 等都可以通过该表达式解析
 *
 * Return:void
 */
void exprState()
{
    ExprType e = exprComplex();
    if (lex.token.type != '=')
    {
        return;
    }
    nextToken();
    exprRvalue();
    switch (e.flag)
    {
    case 1:
    {
        storeVar(e.str);
        break;
    }
    case 2:
    {
        saveConst(OP_STORE_ATTR, obj_to_val(e.str));
        break;
    }
    case 4:
    {
        saveIncodeOp(OP_STORE_INDEX);
        break;
    }
    default:
    {
        syntaxError("assignment error");
        break;
    }
    }
}
/*
 * varState()  - 全局变量赋值语句
 *
 * 范式:varState -> var IDENTIFIER '=' exprRvalue
 * 例如：var a = 2+3*5 ;该语句变量即使是在函数内被赋值，变量依然是全局变量
 *
 * Return:void
 */
void varState()
{
    checkToken(TOKEN_IDENTIFIER);
    StringObject *name = lex.token.u.str;
    checkSkip('=');
    exprRvalue();
    saveConst(OP_STORE_GLOBAL, obj_to_val(name));
}
void statements();
/*
 * blockState()  - 语句块
 *
 * 范式:blockState -> '{' statements '}'
 *
 * Return:void
 */
void blockState()
{
    checkSkip('{');
    while (lex.token.type != '}')
    {
        statements();
    }
    checkSkip('}');
}
/*
 * whileState()  - while循环语句
 *
 * 范式:whileState -> WHILE '(' exprRvalue ')' blockState
 *
 * Return:void
 */
void whileState()
{
    checkSkip('(');
    int top = parser.tabBreak.n;
    tag_label(mark0);
    exprRvalue();
    tag_label(mark1);
    saveIncode(OP_GOTO_IF_FALSE, 0);
    checkSkip(')');
    blockState();
    saveIncode(OP_GOTO, mark0);
    int max = parser.tabBreak.n;
    while (top < max)
    {
        int pos = popIntVec(&parser.tabBreak);
        set_label(pos + 1);
        top++;
    }
    set_label(mark1 + 1);
}
void forOfState(StringObject *name)
{
    nextToken();
    exprRvalue();
    checkSkip(')');
    int arrayPoint = saveNoneVar();
    saveIncode(OP_STORE, arrayPoint);
    saveIncode(OP_LOAD, arrayPoint);
    saveIncodeOp(OP_ARRAY_LENGTH);
    int arrayLength = saveNoneVar();
    saveIncode(OP_STORE, arrayLength);
    saveIncode(OP_CONST_INT, 0);
    int counter = saveNoneVar();
    saveIncode(OP_STORE, counter);
    tag_label(mark0);
    saveIncode(OP_LOAD, counter);
    saveIncode(OP_LOAD, arrayLength);
    saveIncodeOp(OP_LT);
    tag_label(mark1);
    saveIncode(OP_GOTO_IF_FALSE, 0);
    saveIncode(OP_LOAD, arrayPoint);
    saveIncode(OP_LOAD, counter);
    saveIncodeOp(OP_INDEX);
    storeVar(name);
    blockState();
    saveIncode(OP_PLUS_SELF, counter);
    saveIncode(OP_GOTO, mark0);
    set_label(mark1 + 1);
}
void forSubSate()
{
    tag_label(mark0);
    int mark1 = -1;
    int num = -1;
    if (lex.token.type == ';')
    {
        nextToken();
    }
    else
    {
        exprRvalue();
        mark1 = parser.curr->inCode.n;
        saveIncode(OP_GOTO_IF_FALSE, 0);
        checkSkip(';');
    }
    if (lex.token.type == ')')
    {
        nextToken();
    }
    else
    {
        tag_label(top);
        exprState();
        num = parser.curr->inCode.n - top;
        int i;
        for (i = 0; i < num; i++)
        {
            int code = popIntVec(&parser.curr->inCode);
            int line = popIntVec(&parser.curr->lines);
            pushIntVec(&parser.tabFor, code);
            pushIntVec(&parser.tabLine, line);
        }
        checkSkip(')');
    }
    blockState();
    if (num != -1)
    {
        int i;
        for (i = 0; i < num; i++)
        {
            int code = popIntVec(&parser.tabFor);
            int line = popIntVec(&parser.tabLine);
            pushIntVec(&parser.curr->inCode, code);
            pushIntVec(&parser.curr->lines, line);
        }
    }
    saveIncode(OP_GOTO, mark0);
    if (mark1 != -1)
    {
        set_label(mark1 + 1);
    }
}
/*
 * forState()  - for循环语句
 *
 * 范式1:forState -> for '(' IDENTIFIER OF exprRvalue ')' blockState
 * 范式2:forState -> for '(' [ IDENTIFIER '=' exprRvalue ] ';' [ exprState ] ';' [ exprState ] ')' blockState
 * for循环语句包含两个范式，第一个范式是for of语句，第二个范式是for循环常规的用法
 *
 * Return:void
 */
void forState()
{
    checkSkip('(');
    int top = parser.tabBreak.n;
    if (lex.token.type == TOKEN_IDENTIFIER)
    {
        StringObject *name = lex.token.u.str;
        nextToken();
        if (lex.token.type == TOKEN_OF)
        {
            forOfState(name);
        }
        else if (lex.token.type == '=')
        {
            nextToken();
            exprRvalue();
            storeVar(name);
            checkSkip(';');
            forSubSate();
        }
        else
        {
            syntaxError("an error occurred here");
        }
    }
    else if (lex.token.type == ';')
    {
        nextToken();
        forSubSate();
    }
    else
    {
        syntaxError("an error occurred here");
    }
    int max = parser.tabBreak.n;
    while (top < max)
    {
        int pos = popIntVec(&parser.tabBreak);
        set_label(pos + 1);
        top++;
    }
}
/*
 * classBody()  - class语句主体块
 *
 * 范式:classBody -> '{' { IDENTIFIER '=' exprRvalue | IDENTIFIER exprLambda [ ';' ] } '}'
 *
 * Return:class语句成员变量的数量
 */
int classBody()
{
    checkSkip('{');
    int cnt = 0;
    while (lex.token.type == TOKEN_IDENTIFIER)
    {
        StringObject *name = lex.token.u.str;
        saveConst(OP_PUSH, obj_to_val(name));
        nextToken();
        switch (lex.token.type)
        {
        case '=':
        {
            nextToken();
            exprRvalue();
            break;
        }
        case '(':
        {
            exprLambda(name);
            break;
        }
        default:
        {
            syntaxError("an error occurred here");
        }
        }
        if (lex.token.type == ';')
        {
            nextToken();
        }
        cnt++;
    }
    checkSkip('}');
    return cnt;
}
/*
 * classState()  - class语句
 *
 * 范式:classState -> class IDENTIFIER [ EXTENDS IDENTIFIER ] classBody
 *
 * Return:void
 */
void classState()
{
    if (parser.curr != parser.top)
    {
        syntaxError("class must be defined at the global");
    }
    checkToken(TOKEN_IDENTIFIER);
    StringObject *name = lex.token.u.str;
    nextToken();
    StringObject *superName = NULL;
    if (lex.token.type == TOKEN_EXTENDS)
    {
        nextToken();
        checkToken(TOKEN_IDENTIFIER);
        superName = lex.token.u.str;
        nextToken();
    }
    int cnt = classBody();
    saveIncode(OP_CONST_INT, cnt);
    if (superName == NULL)
    {
        saveIncodeOp(OP_CONST_NULL);
    }
    else
    {
        saveConst(OP_LOAD_GLOBAL, obj_to_val(superName));
    }
    saveConst(OP_BUILD_CLASS, obj_to_val(name));
}

/*
 * funcState()  - 函数语句
 *
 * 范式:funcState -> function IDENTIFIER exprLambda
 *
 * Return:void
 */
void funcState()
{
    checkToken(TOKEN_IDENTIFIER);
    StringObject *name = lex.token.u.str;
    nextToken();
    exprLambda(name);
    storeVar(name);
}
/*
 * ifState()  - if语句
 *
 * 范式:ifState -> if '(' exprRvalue ')' blockState { else if '(' exprRvalue ')' blockState } [ else blockState ]
 *
 * Return:void
 */
void ifState()
{
    checkSkip('(');
    exprRvalue();
    tag_label(mark0);
    saveIncode(OP_GOTO_IF_FALSE, 0);
    checkSkip(')');
    blockState();
    tag_label(mark1);
    saveIncode(OP_GOTO, 0);
    set_label(mark0 + 1);
    int flag = 0;
    int top = parser.tabElse.n;
    while (lex.token.type == TOKEN_ELSE)
    {
        nextToken();
        if (lex.token.type == '{')
        {
            if (flag == 1)
            {
                syntaxError("an error occurred here");
            }
            flag = 1;
            blockState();
            break;
        }
        while (lex.token.type == TOKEN_IF)
        {
            if (flag == 1)
            {
                syntaxError("an error occurred here");
            }
            nextToken();
            checkSkip('(');
            exprRvalue();
            tag_label(mark2);
            saveIncode(OP_GOTO_IF_FALSE, 0);
            checkSkip(')');
            blockState();
            tag_label(mark3);
            pushIntVec(&parser.tabElse, mark3);
            saveIncode(OP_GOTO, 0);
            set_label(mark2 + 1);
            break;
        }
    }
    int max = parser.tabElse.n;
    while (top < max)
    {
        int pos = popIntVec(&parser.tabElse);
        set_label(pos + 1);
        top++;
    }
    set_label(mark1 + 1);
}
/*
 * breakState()  - break语句
 *
 * 范式:breakState ->  break ';'
 *
 * Return:void
 */
void breakState()
{
    tag_label(mark);
    saveIncode(OP_GOTO, 0);
    pushIntVec(&parser.tabBreak, mark);
    checkSkip(';');
}
/*
 * returnState()  - return语句
 *
 * 范式:returnState ->  return [ exprRvalue ] ';'
 *
 * Return:void
 */
void returnState()
{
    if (lex.token.type == ';' || lex.token.type == '}')
    {
        saveIncodeOp(OP_CONST_NULL);
    }
    else
    {
        exprRvalue();
    }
    saveIncodeOp(OP_RETURN);
    checkSkip(';');
}
/*
 * statements()  - 语句主体
 *
 * 范式:statements ->  ifState | whileState | forState | classState | funcState | breakState | returnState | varState | exprState
 *
 * Return:void
 */
void statements()
{
    switch (lex.token.type)
    {
    case TOKEN_IF:
    {
        nextToken();
        ifState();
        break;
    }
    case TOKEN_WHILE:
    {
        nextToken();
        whileState();
        break;
    }
    case TOKEN_FOR:
    {
        nextToken();
        forState();
        break;
    }
    case TOKEN_CLASS:
    {
        nextToken();
        classState();
        break;
    }
    case TOKEN_FUNCTION:
    {
        nextToken();
        funcState();
        break;
    }
    case TOKEN_BREAK:
    {
        nextToken();
        breakState();
        break;
    }
    case TOKEN_RETURN:
    {
        nextToken();
        returnState();
        break;
    }
    case TOKEN_VAR:
    {
        nextToken();
        varState();
        break;
    }
    default:
    {
        exprState();
        checkSkip(';');
        break;
    }
    }
}
/*
 * startParser()  - 开始语法分析的入口
 *
 * 循环执行statements
 *
 * Return:void
 */
void startParser()
{
    nextToken();
    while (lex.token.type != TOKEN_EOF)
    {
        statements();
    }
    saveIncodeOp(OP_CONST_NULL);
    saveIncodeOp(OP_RETURN);
}
/*
 * constString(char * tempStr,Value v) - 常量转字符串
 * @tempStr:字符串
 * @v:常量
 *
 * 函数根据常量的值，转化成对应的字符串,常量只包含三种类型，数值型、字符串型和编译环境型
 *
 * Return:void
 */
void constString(char *tempStr, Value v)
{
    if (v.type == NUMBER)
    {
        sprintf(tempStr, "NUMBER:%.14g", v.u.number);
        return;
    }
    if (v.type == OBJECT)
    {
        if (v.u.object->type == STRING_OBJ)
        {
            char *s = ((StringObject *)v.u.object)->str;
            snprintf(tempStr, 1024, "STRING:%s", s);
            return;
        }
        if (v.u.object->type == COMPILER_OBJ)
        {
            char *s = ((MyCompiler *)v.u.object)->name->str;
            snprintf(tempStr, 1024, "COMPILER:%s", s);
            return;
        }
    }
    printf("value type is error");
    exit(1);
}
/*
 * showCompiler(MyCompiler *c) - 展示编译环境的字节码信息
 * @c:编译环境
 *
 * 循环当前编译环境的字节码输出对应的信息
 * 第1列：字节码操作值所在的行
 * 第2列：操作值在字节码的位置，goto跳转时会用到
 * 第3列：操作码
 * 第4列：操作值
 * 第5列：操作值对应的含义，[]中括起来的内容是常量，<>中括起来的内容是变量
 *
 * Return:void
 */
void showCompiler(MyCompiler *c)
{
    char tempStr[1024];
    printf(">>>complier module is:%s,arguments number is %d \n", c->name->str, c->argsNum);
    printf(">>>complier module code: \n");
    Value *consts = c->consts.data;
    int *inData = c->inCode.data;
    int line, v, ip;
    char *str;
    for (ip = 0; ip < c->inCode.n; ip++)
    {
        line = c->lines.data[ip];
        switch (inData[ip])
        {
        case OP_PUSH:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_PUSH", v, tempStr);
            break;
        }
        case OP_LOAD:
        {
            v = OP_VALUE;
            if (c->var.data[v] == NULL)
            {
                str = "NULL";
            }
            else
            {
                str = ((StringObject *)(c->var.data[v]))->str;
            }
            printf("%-6d|%-6d|%-20s %-6d <%s> \n", line, ip, "OP_LOAD", v, str);
            break;
        }
        case OP_LOAD_GLOBAL:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_LOAD_GLOBAL", v, tempStr);
            break;
        }
        case OP_STORE_GLOBAL:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_STORE_GLOBAL", v, tempStr);
            break;
        }
        case OP_STORE:
        {
            v = OP_VALUE;
            if (c->var.data[v] == NULL)
            {
                str = "NULL";
            }
            else
            {
                str = ((StringObject *)(c->var.data[v]))->str;
            }
            printf("%-6d|%-6d|%-20s %-6d <%s> \n", line, ip, "OP_STORE", v, str);
            break;
        }
        case OP_CONST_NULL:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_CONST_NULL");
            break;
        }
        case OP_CONST_FALSE:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_CONST_FALSE");
            break;
        }
        case OP_CONST_TRUE:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_CONST_TRUE");
            break;
        }
        case OP_CONST_INT:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_CONST_INT", v);
            break;
        }
        case OP_BUILD_CLASS:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_BUILD_CLASS", v, tempStr);
            break;
        }
        case OP_BUILD_OBJECT:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_BUILD_OBJECT", v);
            break;
        }
        case OP_BUILD_MAP:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_BUILD_MAP", v);
            break;
        }
        case OP_BUILD_LIST:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_BUILD_LIST", v);
            break;
        }
        case OP_PLUS_SELF:
        {
            v = OP_VALUE;
            if (c->var.data[v] == NULL)
            {
                str = "NULL";
            }
            else
            {
                str = ((StringObject *)(c->var.data[v]))->str;
            }
            printf("%-6d|%-6d|%-20s %-6d <%s> \n", line, ip, "OP_PLUS_SELF", v, str);
            break;
        }
        case OP_FUNC_CALL:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_FUNC_CALL", v);
            break;
        }
        case OP_METHOD_CALL:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_METHOD_CALL", v, tempStr);
            break;
        }
        case OP_RETURN:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_RETURN");
            break;
        }
        case OP_LOAD_ATTR:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_LOAD_ATTR", v, tempStr);
            break;
        }
        case OP_STORE_ATTR:
        {
            v = OP_VALUE;
            constString(tempStr, consts[v]);
            printf("%-6d|%-6d|%-20s %-6d [%s] \n", line, ip, "OP_STORE_ATTR", v, tempStr);
            break;
        }
        case OP_THIS:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_THIS");
            break;
        }
        case OP_SUPER:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_SUPER");
            break;
        }
        case OP_PLUS:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_PLUS");
            break;
        }
        case OP_SUB:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_SUB");
            break;
        }
        case OP_MUL:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_MUL");
            break;
        }
        case OP_DIV:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_DIV");
            break;
        }
        case OP_EQ:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_EQ");
            break;
        }
        case OP_NOT_EQ:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_NOT_EQ");
            break;
        }
        case OP_OR:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_OR");
            break;
        }
        case OP_AND:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_AND");
            break;
        }
        case OP_LT_EQ:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_LT_EQ");
            break;
        }
        case OP_GT_EQ:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_GT_EQ");
            break;
        }
        case OP_GT:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_GT");
            break;
        }
        case OP_LT:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_LT");
            break;
        }
        case OP_NOT:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_NOT");
            break;
        }
        case OP_MINUS:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_MINUS");
            break;
        }
        case OP_GOTO_IF_FALSE:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_GOTO_IF_FALSE", v);
            break;
        }
        case OP_GOTO:
        {
            v = OP_VALUE;
            printf("%-6d|%-6d|%-20s %-6d \n", line, ip, "OP_GOTO", v);
            break;
        }
        case OP_INDEX:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_INDEX");
            break;
        }
        case OP_STORE_INDEX:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_STORE_INDEX");
            break;
        }
        case OP_ARRAY_LENGTH:
        {
            printf("%-6d|%-6d|%s \n", line, ip, "OP_ARRAY_LENGTH");
            break;
        }
        default:
        {
            printf("unreachable");
            exit(1);
        }
        }
    }
}
/*
 * showParser(MyCompiler *top) - 展示全局编译的字节码信息
 * @c:全局编译环境
 *
 * Return:void
 */
void showParser(MyCompiler *top)
{
    showCompiler(top);
    int i;
    for (i = 0; i < top->consts.n; i++)
    {
        Value v = top->consts.data[i];
        if (v.type == OBJECT && v.u.object->type == COMPILER_OBJ)
        {
            printf("\n");
            showCompiler((MyCompiler *)v.u.object);
        }
    }
}
/*********************
 * 以下是虚拟机执行的部分
 *********************
 */
/*
 * struct HashNode - 哈希map节点
 * @string：字符串
 * @v：值
 * @next：下一个节点
 */
typedef struct HashNode_
{
    StringObject *string;
    Value v;
    struct HashNode_ *next;
} HashNode;
/*
 * struct HashMap - 哈希map
 * @cap：哈希map容量
 * @n：元素数量
 * @hashNode：节点集合
 */
typedef struct HashMap_
{
    int cap;
    int n;
    HashNode **hashNode;
} HashMap;
/*
 * struct MapObject - Map对象
 * @OBJECT_HEAD：对象头
 * @map：哈希map
 */
typedef struct MapObject_
{
    OBJECT_HEAD
    HashMap map;
} MapObject;
/*
 * struct ClassObject - class对象
 * @OBJECT_HEAD：对象头
 * @fields：成员变量
 * @name：对象名
 * @super：父类
 */
typedef struct ClassObject_
{
    OBJECT_HEAD
    HashMap fields;
    StringObject *name;
    struct ClassObject_ *super;
} ClassObject;
/*
 * struct InstanceObject - 实例对象
 * @OBJECT_HEAD：对象头
 * @fields：成员变量
 * @inClass：实例对象所属类
 */
typedef struct InstanceObject_
{
    OBJECT_HEAD
    HashMap fields;
    ClassObject *inClass;
} InstanceObject;
/*
 * struct ListObject - list对象
 * @OBJECT_HEAD：对象头
 * @list：值数组
 */
typedef struct ListObject_
{
    OBJECT_HEAD
    VecValue list;
} ListObject;
/*
 * struct MemberObject - 成员对象
 * @OBJECT_HEAD：对象头
 * @caller：调用者
 * @callee：被调用者
 */
typedef struct MemberObject_
{
    OBJECT_HEAD
    MyObject *caller;
    MyObject *callee;
} MemberObject;
/*
 * struct BuildinObject - 内建函数对象
 * @OBJECT_HEAD：对象头
 * @inFunc：指向函数的指针，argsNum是输入参数，args是参数值，返回值是调用是否成功
 */
typedef struct BuildinObject_
{
    OBJECT_HEAD
    int (*inFunc)(int argsNum, Value *args);
} BuildinObject;
/*
 * struct GC - 标记清除垃圾回收结构体
 * @head：对象链表的头，可以通过该链表找到所有对象
 * @cap：gc容量
 * @n：对象的数量，当对象大小超过gc容量大小，需要执行一次垃圾回收，并扩容gc容量
 * @stringLink：空闲string对象链表，执行完垃圾回收会把需要删除的对象回收到这个链表中，
 * 下次创建同样的对象时可以不用重新分配内存，从这个链表中把对象取出进行初始化
 * @listLink：空闲list对象链表
 * @mapLink：空闲map对象链表
 * @insLink：空闲Instance对象链表
 * @memLink：空闲Member对象链表
 * @classLink：空闲class对象链表
 * @nodeLink：空闲节点链表
 */
typedef struct GC_
{
    MyObject *head;
    int cap;
    int n;
    MyObject *stringLink;
    MyObject *listLink;
    MyObject *mapLink;
    MyObject *insLink;
    MyObject *memLink;
    MyObject *classLink;
    HashNode *nodeLink;
} GC;
/*
 * struct MyVm - 虚拟机
 * @top：全局编译环境
 * @stack：虚拟机栈
 * @topStack：栈顶
 * @globals：全局变量
 * @gc：垃圾回收
 * @stringClass：字符串类
 * @listClass:list类
 * @mapClass：map类
 * @constructorStr：初始函数字符串
 */
typedef struct MyVm_
{
    MyCompiler *top;
    Value stack[MAX_STACK];
    int topStack;
    HashMap globals;
    GC gc;
    ClassObject *stringClass;
    ClassObject *listClass;
    ClassObject *mapClass;
    StringObject *constructorStr;
} MyVm;

MyVm vm; /*虚拟机全局变量*/

MyObject *gcMalloc(int size);

#define swap_gc_link(type, link) \
    obj = (type)link;            \
    link = link->next;           \
    init_object_head(obj)

/*
 * initHashMap(HashMap *obj, int size) - 初始化哈希map
 * @obj:哈希map
 * @size:容量大小
 *
 * Return:void
 */
void initHashMap(HashMap *obj, int size)
{
    obj->cap = size;
    obj->n = 0;
    obj->hashNode = (HashNode **)malloc(sizeof(HashNode *) * size);
    int i;
    for (i = 0; i < size; i++)
    {
        obj->hashNode[i] = NULL;
    }
}
/*
 * initHashNode(StringObject *str, Value v, HashNode *next) - 初始化哈希map节点
 * @str:字符串
 * @v:值
 * @next：下一个节点
 *
 * 首先判断空闲列表nodeLink是否是空的，如果不是空就从空闲列表中取出对象进行初始化
 * 如果空闲列表是空的通过malloc创建内存进行初始化
 *
 * Return:初始化后的节点
 */
HashNode *initHashNode(StringObject *str, Value v, HashNode *next)
{
    HashNode *obj = NULL;
    if (vm.gc.nodeLink != NULL)
    {
        obj = vm.gc.nodeLink;
        vm.gc.nodeLink = vm.gc.nodeLink->next;
        obj->next = NULL;
    }
    else
    {
        obj = (HashNode *)malloc(sizeof(HashNode));
    }
    obj->string = str;
    obj->v = v;
    obj->next = next;
    return obj;
}
/*
 * getHashNode(HashMap *obj, StringObject *str, int pos) - 通过字符串及位置从哈希map获得节点信息
 * @obj:哈希map
 * @str:字符串
 * @pos:位置
 *
 * 首先通过pos找到该字符串所在节点信息，如果循环节点所在的链表对比字符串哈希值找到字符串所在的节点
 * 如果没有查找到返回空
 *
 * Return:节点
 */
HashNode *getHashNode(HashMap *obj, StringObject *str, int pos)
{
    HashNode *tmp;
    for (tmp = obj->hashNode[pos]; tmp != NULL; tmp = tmp->next)
    {
        if (tmp->string->hash == str->hash)
        {
            return tmp;
        }
    }
    return NULL;
}
/*
 * getHashMap(HashMap *obj, StringObject *str)- 通过字符串从哈希map获得节点信息
 * @obj:哈希map
 * @str:字符串
 *
 * 首先通过对字符串哈希值和哈希map的容量求余，获得字符串在哈希map中的位置，调用getHashNode获得节点信息
 *
 * Return:节点
 */
HashNode *getHashMap(HashMap *obj, StringObject *str)
{
    int pos = str->hash % obj->cap;
    HashNode *node = getHashNode(obj, str, pos);
    return node;
}
/*
 * saveHashNode(HashMap *obj, StringObject *str, Value v)- 保存字符串及值到哈希map
 * @obj:哈希map
 * @str:字符串
 * @v:值
 *
 * 首先判断哈希map中时候有该字符串，如果有把值写入该节点的值字段中
 * 然后判断哈希map中元素的数量，如果元素数量大于容量的3倍，把容量扩容3倍，并把原来的节点重新排列重组
 * 最后把字符串及值插入到对应的节点
 *
 * Return:节点
 */
HashNode *saveHashNode(HashMap *obj, StringObject *str, Value v)
{
    int pos = str->hash % obj->cap;
    HashNode *node = getHashNode(obj, str, pos);
    if (node != NULL)
    {
        node->v = v;
        return node;
    }
    if (obj->n > obj->cap * 3)
    {
        int oldCap = obj->cap;
        int newCap = oldCap * 3;
        HashNode **old = obj->hashNode;
        HashNode **new = (HashNode **)malloc(sizeof(HashNode *) * newCap);
        int i;
        HashNode *tmp, *curr;
        for (i = 0; i < obj->cap; i++)
        {
            curr = old[i];
            while (curr != NULL)
            {
                tmp = curr;
                curr = curr->next;
                int newPos = tmp->string->hash % newCap;
                tmp->next = new[newPos];
                new[newPos] = tmp;
            }
        }
        free(old);
        obj->hashNode = new;
        obj->cap = newCap;
        pos = str->hash % obj->cap;
        obj->hashNode[pos] = initHashNode(str, v, obj->hashNode[pos]);
        obj->n++;
        return obj->hashNode[pos];
    }
    else
    {
        obj->hashNode[pos] = initHashNode(str, v, obj->hashNode[pos]);
        obj->n++;
        return obj->hashNode[pos];
    }
}
/*
 * initStringObject(char *str) - 初始化字符串对象
 * @str:字符串
 *
 * 首先判断空闲列表stringLink是否是空的，如果不是空就从空闲列表中取出对象进行初始化
 * 如果空闲列表是空的通过gcMalloc创建内存进行初始化
 *
 * Return:字符串对象
 */
StringObject *initStringObject(char *str)
{
    StringObject *obj = NULL;
    if (vm.gc.stringLink != NULL)
    {
        swap_gc_link(StringObject *, vm.gc.stringLink);
    }
    else
    {
        obj = (StringObject *)gcMalloc(sizeof(StringObject));
    }
    obj->type = STRING_OBJ;
    obj->len = strlen(str);
    obj->hash = utilKey2Hash(str, obj->len);
    obj->str = str;
    return obj;
}
/*
 * initClass(StringObject *name, ClassObject *super, int count) - 初始化class对象
 * @name:类名
 * @super:父类
 * @count:成员数量
 *
 * 首先判断空闲列表ClassObject是否是空的，如果不是空就从空闲列表中取出对象进行初始化
 * 如果空闲列表是空的通过gcMalloc创建内存进行初始化
 * 防止传入的成员数是0，成员数量+12作为类成员容量的初始值
 * 如果父类不是空，把父类的成员变量复制到本类中
 *
 * Return:class对象
 */
ClassObject *initClass(StringObject *name, ClassObject *super, int count)
{
    ClassObject *obj = NULL;
    if (vm.gc.classLink != NULL)
    {
        swap_gc_link(ClassObject *, vm.gc.classLink);
    }
    else
    {
        obj = (ClassObject *)gcMalloc(sizeof(ClassObject));
    }
    obj->type = CLASS_OBJ;
    initHashMap(&obj->fields, count + 12);
    obj->name = name;
    obj->super = super;
    if (super != NULL)
    {
        int i;
        HashNode *tmp;
        for (i = 0; i < super->fields.cap; i++)
        {
            for (tmp = super->fields.hashNode[i]; tmp != NULL; tmp = tmp->next)
            {
                int pos = tmp->string->hash % obj->fields.cap;
                obj->fields.hashNode[pos] = initHashNode(tmp->string, tmp->v, obj->fields.hashNode[pos]);
            }
        }
        obj->fields.n = super->fields.n;
    }
    return obj;
}
/*
 * initMemberObject(MyObject *caller, MyObject *callee) - 初始化member对象
 * @caller:调用者
 * @callee:被调用者
 *
 * Return:member对象
 */
MemberObject *initMemberObject(MyObject *caller, MyObject *callee)
{
    MemberObject *obj = NULL;
    if (vm.gc.memLink != NULL)
    {
        swap_gc_link(MemberObject *, vm.gc.memLink);
    }
    else
    {
        obj = (MemberObject *)gcMalloc(sizeof(MemberObject));
    }
    obj->type = MEMBER_OBJ;
    obj->caller = caller;
    obj->callee = callee;
    return obj;
}
/*
 * initInstanceObject(ClassObject *co) - 初始化实例对象
 * @co:实例所属的类
 *
 * Return:实例对象
 */
InstanceObject *initInstanceObject(ClassObject *co)
{
    InstanceObject *obj = NULL;
    if (vm.gc.insLink != NULL)
    {
        swap_gc_link(InstanceObject *, vm.gc.insLink);
    }
    else
    {
        obj = (InstanceObject *)gcMalloc(sizeof(InstanceObject));
    }
    obj->type = INSTANCE_OBJ;
    initHashMap(&obj->fields, co->fields.cap + RESERVE_MAP_CAP);
    obj->inClass = co;
    return obj;
}
/*
 * initMapObject(int count) - 初始化map对象
 * @count:容量
 *
 * Return:map对象
 */
MapObject *initMapObject(int count)
{
    MapObject *obj = NULL;
    if (vm.gc.mapLink != NULL)
    {
        swap_gc_link(MapObject *, vm.gc.mapLink);
    }
    else
    {
        obj = (MapObject *)gcMalloc(sizeof(MapObject));
    }
    obj->type = MAP_OBJ;
    initHashMap(&obj->map, count + RESERVE_MAP_CAP);
    return obj;
}
/*
 * initListObject(int count) - 初始化list对象
 * @count:容量
 *
 * Return:list对象
 */
ListObject *initListObject(int count)
{
    ListObject *obj = NULL;
    if (vm.gc.listLink != NULL)
    {
        swap_gc_link(ListObject *, vm.gc.listLink);
    }
    else
    {
        obj = (ListObject *)gcMalloc(sizeof(ListObject));
    }
    obj->type = LIST_OBJ;
    initValueVec(&obj->list, count);
    obj->list.n = count;
    return obj;
}
void registerStdlib();
/*
 * initGc(GC *gc) - 初始化垃圾回收gc
 * @gc:垃圾回收
 *
 * Return:void
 */
void initGc(GC *gc)
{
    gc->cap = INIT_GC_CAP;
    gc->n = 0;
    gc->head = NULL;
    gc->stringLink = NULL;
    gc->listLink = NULL;
    gc->mapLink = NULL;
    gc->insLink = NULL;
    gc->memLink = NULL;
    gc->classLink = NULL;
    gc->nodeLink = NULL;
}
ClassObject *stdAddClass(char *name);
/*
 * initVm(MyCompiler *top) - 初始化虚拟机
 * @top:全局编译环境
 *
 * Return:void
 */
void initVm(MyCompiler *top)
{
    vm.top = top;
    vm.topStack = 0;
    initHashMap(&vm.globals, GLOBAL_VARS);
    initGc(&vm.gc);
    vm.stringClass = stdAddClass("String");
    vm.listClass = stdAddClass("List");
    vm.mapClass = stdAddClass("Map");
    vm.constructorStr = initStringObject(utilStrdup("constructor"));
    registerStdlib();
}
/*
 * pushStack(Value value) - 向虚拟栈栈顶插入数据
 * @value:值
 *
 * Return:void
 */
void pushStack(Value value)
{
    vm.stack[vm.topStack] = value;
    vm.topStack++;
}
/*
 * popStack() - 从虚拟栈栈顶弹出元素
 *
 * Return:值
 */
Value popStack()
{
    vm.topStack--;
    return vm.stack[vm.topStack];
}
int startVm(MyCompiler *c, int start);
/*
 * concatString(Value l, Value r) - 拼接两个字符串对象成一个字符串对象
 * @l：左值
 * @r：右值
 *
 * Return:字符串对象
 */
StringObject *concatString(Value l, Value r)
{
    StringObject *s1 = (StringObject *)l.u.object;
    StringObject *s2 = (StringObject *)r.u.object;
    char *tmp = (char *)malloc(s1->len + s1->len + 1);
    strcpy(tmp, s1->str);
    strcat(tmp, s2->str);
    StringObject *str = initStringObject(tmp);
    return str;
}
/*
 * concatList(Value l, Value r) - 拼接两个list对象成一个list对象
 * @l：左值
 * @r：右值
 *
 * Return:list对象
 */
ListObject *concatList(Value l, Value r)
{
    ListObject *l1 = (ListObject *)l.u.object;
    ListObject *l2 = (ListObject *)r.u.object;
    int count = l1->list.n + l2->list.n;
    ListObject *obj = initListObject(count);
    int i, j;
    for (i = 0; i < l1->list.n; i++)
    {
        obj->list.data[i] = l1->list.data[i];
    }
    for (j = 0; j < l2->list.n; j++)
    {
        obj->list.data[i + j] = l2->list.data[j];
    }
    return obj;
}
/*
 * setError(char *errMsg) - 输出错误信息并返回错误标识
 * @errMsg：错误信息
 *
 * Return:错误标识，1代表错误
 */
int setError(char *errMsg)
{
    printf("runTimeError:%s \n", errMsg);
    return 1;
}
/*
 * funcEntry(MyObject *obj, int this, int argsNum) - 方法执行的入口
 * @obj：函数对象
 * @this:函数在虚拟栈的位置
 * @argsNum：函数参数
 *
 * 如果函数对象是编译型对象调用startVm执行，如果是内建函数调用指向函数的指针
 *
 * Return:错误标识
 */
int funcEntry(MyObject *obj, int this, int argsNum)
{
    int res = 0;
    switch (obj->type)
    {
    case COMPILER_OBJ:
    {
        if (((MyCompiler *)obj)->argsNum != argsNum)
        {
            return setError("argument error");
        }
        res = startVm((MyCompiler *)obj, this);
        break;
    }
    case BUILDIN_OBJ:
    {
        res = ((BuildinObject *)obj)->inFunc(argsNum, &(vm.stack[this]));
        vm.topStack = this + 1;
        break;
    }
    default:
    {
        return setError("object can not call");
    }
    }
    return res;
}
/*
 * classFunc(ClassObject *co, StringObject *name, int this, int argsNum) - 从class中找成员变量并调用
 * @co：对象所属的类
 * @name:成员变量名
 * @this：函数在虚拟栈的位置
 * @argsNum：函数参数
 *
 * 从对象所属的类中找是否存在成员变量，如果存在则调用该成员方法
 *
 * Return:错误标识
 */
int classFunc(ClassObject *co, StringObject *name, int this, int argsNum)
{
    HashNode *node = getHashMap(&co->fields, name);
    if (node == NULL)
    {
        return setError("method not found in class");
    }
    if (node->v.type != OBJECT)
    {
        return setError("type error");
    }
    return funcEntry(node->v.u.object, this, argsNum);
}
/*
 * functionCall(int argsNum) - 函数调用
 * @argsNum：函数参数
 *
 * 判断待调起的对象是否是MemberObject对象，如果是函数执行位置改成调起者，把函数对象改成被调起者
 * 如果不是MemberObject对象，调用funcEntry方法
 *
 * Return:错误标识
 */
int functionCall(int argsNum)
{
    int this = vm.topStack - argsNum - 1;
    Value val = vm.stack[this];
    if (val.type != OBJECT)
    {
        return setError("object can not call");
    }
    MyObject *obj = val.u.object;
    if (obj->type == MEMBER_OBJ)
    {
        MemberObject *m = (MemberObject *)obj;
        vm.stack[this] = obj_to_val(m->caller);
        return funcEntry(m->callee, this, argsNum);
    }
    else
    {
        return funcEntry(obj, this, argsNum);
    }
}
/*
 * methodCall(StringObject *name, int argsNum) - 方法调用
 * @name：成员变量名
 * @argsNum：函数参数
 *
 * 判断待调起的值是否是super，如果是，把函数执行位置改成super对应的实例对象，执行classFunc方法
 * 判断待调起的对象是否是字符串对象或者列表对象或者map对象，如果是，在对象所属的类中查到变量名，并执行相应的操作
 * 判断待调起的对象是否实例，如果是，通过成员变量名在实例中查到，如果在实例中查找不到在实例所属的类中查找，并执行相应的操作
 *
 * Return:错误标识
 */
int methodCall(StringObject *name, int argsNum)
{
    int this = vm.topStack - argsNum - 1;
    Value val = vm.stack[this];
    if (val.type == SUPER)
    {
        vm.stack[this] = obj_to_val(val.u.object);
        InstanceObject *ins = (InstanceObject *)val.u.object;
        if (ins->inClass->super == NULL)
        {
            return setError("super is null object");
        }
        return classFunc(ins->inClass->super, name, this, argsNum);
    }
    if (val.type != OBJECT)
    {
        return setError("can not execute method");
    }
    switch (val.u.object->type)
    {
    case STRING_OBJ:
    {
        return classFunc(vm.stringClass, name, this, argsNum);
    }
    case LIST_OBJ:
    {
        return classFunc(vm.listClass, name, this, argsNum);
    }
    case MAP_OBJ:
    {
        return classFunc(vm.mapClass, name, this, argsNum);
    }
    case INSTANCE_OBJ:
    {
        InstanceObject *ins = (InstanceObject *)val.u.object;
        HashNode *node = getHashMap(&ins->fields, name);
        if (node == NULL)
        {
            return classFunc(ins->inClass, name, this, argsNum);
        }
        else
        {
            if (node->v.type != OBJECT)
            {
                return setError("type error");
            }
            return funcEntry(node->v.u.object, this, argsNum);
        }
    }
    default:
    {
        return setError("object can not execute method");
    }
    }
}
int stdString(int argsNum, Value *args);
/*
 * stdList(int argsNum, Value *args) - 通过字面的方式创建list对象
 * @argsNum：参数数量
 * @args：参数值
 *
 * 判断参数数量，如果数量是空，list的容量设置成0
 * 如果参数是1，读取参数值，把参数值设置成list的容量
 *
 * Return:错误标识
 */
int stdList(int argsNum, Value *args)
{
    int n;
    switch (argsNum)
    {
    case 0:
        n = 0;
        break;
    case 1:
    {
        Value v = args[1];
        if (v.type != NUMBER)
        {
            return setError("argument error");
        }
        n = (int)v.u.number;
        break;
    }
    default:
        return setError("argument error");
    }
    ListObject *lst = initListObject(n);
    int i;
    for (i = 0; i < n; i++)
    {
        lst->list.data[i] = (Value){NONE};
    }
    args[0] = obj_to_val((MyObject *)lst);
    return 0;
}
/*
 * stdMap(int argsNum, Value *args) - 通过字面的方式创建map对象
 * @argsNum：参数数量
 * @args：参数值
 *
 * 判断参数数量，如果数量是空，map的容量设置成0
 * 如果参数是1，读取参数值，把参数值设置成map的容量
 *
 * Return:错误标识
 */
int stdMap(int argsNum, Value *args)
{
    int n;
    switch (argsNum)
    {
    case 0:
        n = 0;
        break;
    case 1:
    {
        Value v = args[1];
        if (v.type != NUMBER)
        {
            return setError("argument error");
        }
        n = (int)v.u.number;
        break;
    }
    default:
        return setError("argument error");
    }
    MyObject *obj = (MyObject *)initMapObject(n);
    args[0] = obj_to_val(obj);
    return 0;
}
/*
 * buildObject(ClassObject *co, int this, int argsNum) - 创建对象
 * @co：对象所属的类
 * @this：函数在虚拟栈的位置
 * @argsNum：函数参数
 *
 * 判断co是否是stringClass或者listClass或者mapClass，如果是创建相应的对象
 * 如果不是，则创建实例对象，并查到实例对象中是否包含constructor成员变量，如果不包含且参数大约0说明参数有误
 * 如果包含constructor，则通过调用startVm初始化实例
 *
 * Return:错误标识
 */
int buildObject(ClassObject *co, int this, int argsNum)
{
    if (co == vm.stringClass)
    {
        return stdString(argsNum, &vm.stack[this]);
    }
    if (co == vm.listClass)
    {
        return stdList(argsNum, &vm.stack[this]);
    }
    if (co == vm.mapClass)
    {
        return stdMap(argsNum, &vm.stack[this]);
    }
    MyObject *obj = (MyObject *)initInstanceObject(co);
    vm.stack[this] = obj_to_val(obj);
    HashNode *node = getHashMap(&co->fields, vm.constructorStr);
    if (node == NULL && argsNum != 0)
    {
        return setError("arguments error");
    }
    if (node != NULL)
    {
        if (node->v.type != OBJECT && node->v.u.object->type != COMPILER_OBJ)
        {
            return setError("constructor must be COMPILER_OBJ type");
        }
        int res = startVm((MyCompiler *)node->v.u.object, this);
        vm.stack[this] = obj_to_val(obj);
        vm.topStack = this + 1;
        return res;
    }
    return 0;
}
/*
 * runTimeError(StringObject *name, int line, char *errMsg) - 运行时错误信息
 * @name：编译环境名称
 * @line：行信息
 * @errMsg：错误信息
 *
 * Return:错误标识
 */
int runTimeError(StringObject *name, int line, char *errMsg)
{
    if (errMsg != NULL)
    {
        setError(errMsg);
    }
    printf(">>>>>> line [%d],in %s \n", line, name->str);
    return 1;
}

#define run_time_error(msg) runTimeError(c->name, c->lines.data[ip], msg)

#define load_attr_obj(nodeVal)                                                                 \
    do                                                                                         \
    {                                                                                          \
        if (nodeVal.type == OBJECT &&                                                          \
            (nodeVal.u.object->type == COMPILER_OBJ || nodeVal.u.object->type == BUILDIN_OBJ)) \
        {                                                                                      \
            MemberObject *mo = initMemberObject(val.u.object, nodeVal.u.object);               \
            vm.stack[vm.topStack - 1] = obj_to_val(mo);                                        \
        }                                                                                      \
        else                                                                                   \
        {                                                                                      \
            vm.stack[vm.topStack - 1] = nodeVal;                                               \
        }                                                                                      \
    } while (0)

#define exp_is_bool(left, right) \
    (left.type == TRUE || left.type == FALSE) && (right.type == TRUE || right.type == FALSE)

#define exp_is_number(left, right)                                \
    do                                                            \
    {                                                             \
        if (left.type != right.type && left.type != NUMBER)       \
        {                                                         \
            return run_time_error("an error occurred in option"); \
        }                                                         \
    } while (0)
/*
 * startVm(MyCompiler *c, int start) - 虚拟机启动
 * @c：编译环境
 * @start：运行时栈的起始位置
 *
 * startVm是函数的执行入口
 * 首先判断编译环境的占用多少运行时栈加上已经占用的数量会不会超过最大运行时栈，如果超过直接报错
 * 循环编译环境的字节码，判断操作码是那种类型进入到不同的分支下面执行
 *
 * Return:错误标识
 */
int startVm(MyCompiler *c, int start)
{
    if (c->maxOccupyStack + vm.topStack >= MAX_STACK)
    {
        printf("runtime stack is full");
        exit(1);
    }
    vm.topStack = vm.topStack + c->var.n + 1;
    Value *consts = c->consts.data;
    int *inData = c->inCode.data;
    int ip = 0;
    while (1)
    {
        switch (inData[ip])
        {
        case OP_PUSH:
        {
            pushStack(consts[OP_VALUE]);
            break;
        }
        case OP_LOAD:
        {
            pushStack(vm.stack[start + OP_VALUE]);
            break;
        }
        case OP_LOAD_GLOBAL:
        {
            StringObject *key = (StringObject *)(consts[OP_VALUE].u.object);
            HashNode *node = getHashMap(&vm.globals, key);
            if (node == NULL)
            {
                return run_time_error("can not find variable");
            }
            pushStack(node->v);
            break;
        }
        case OP_STORE_GLOBAL:
        {
            StringObject *key = (StringObject *)(consts[OP_VALUE].u.object);
            Value v = popStack();
            saveHashNode(&vm.globals, key, v);
            break;
        }
        case OP_STORE:
        {
            vm.stack[start + OP_VALUE] = popStack();
            break;
        }
        case OP_CONST_NULL:
        {
            pushStack((Value){NONE});
            break;
        }
        case OP_CONST_FALSE:
        {
            pushStack((Value){FALSE});
            break;
        }
        case OP_CONST_TRUE:
        {
            pushStack((Value){TRUE});
            break;
        }
        case OP_CONST_INT:
        {
            pushStack(num_to_val(OP_VALUE));
            break;
        }
        case OP_BUILD_CLASS:
        {
            StringObject *name = (StringObject *)(consts[OP_VALUE].u.object);
            ClassObject *parents = NULL;
            Value v1 = popStack();
            if (v1.type != NONE)
            {
                if (v1.u.object->type != CLASS_OBJ ||
                    v1.u.object == (MyObject *)vm.stringClass ||
                    v1.u.object == (MyObject *)vm.listClass ||
                    v1.u.object == (MyObject *)vm.mapClass)
                {
                    return run_time_error("extend object must class object");
                }
                parents = (ClassObject *)v1.u.object;
            }
            int count = (int)(popStack()).u.number;
            ClassObject *obj = initClass(name, parents, count);
            Value key, val;
            int i;
            for (i = 0; i < count; i++)
            {
                val = popStack();
                key = popStack();
                saveHashNode(&obj->fields, (StringObject *)key.u.object, val);
            }
            saveHashNode(&vm.globals, name, obj_to_val(obj));
            break;
        }
        case OP_BUILD_OBJECT:
        {
            int argsNum = OP_VALUE;
            int this = vm.topStack - argsNum - 1;
            Value val = vm.stack[this];
            if (val.type != OBJECT && val.u.object->type != CLASS_OBJ)
            {
                return run_time_error("must class object");
            }
            ClassObject *co = (ClassObject *)val.u.object;
            if (buildObject(co, this, argsNum) != 0)
            {
                return run_time_error(NULL);
            }
            break;
        }
        case OP_BUILD_MAP:
        {
            int count = OP_VALUE;
            MapObject *obj = initMapObject(count);
            Value key, val;
            int i;
            for (i = 0; i < count; i++)
            {
                val = popStack();
                key = popStack();
                saveHashNode(&obj->map, (StringObject *)key.u.object, val);
            }
            pushStack(obj_to_val(obj));
            break;
        }
        case OP_BUILD_LIST:
        {
            int count = OP_VALUE;
            ListObject *obj = initListObject(count);
            int i;
            for (i = 1; i <= count; i++)
            {
                obj->list.data[count - i] = popStack();
            }
            pushStack(obj_to_val(obj));
            break;
        }
        case OP_PLUS_SELF:
        {
            (vm.stack[start + OP_VALUE]).u.number++;
            break;
        }
        case OP_FUNC_CALL:
        {
            int argsNum = OP_VALUE;
            int res = functionCall(argsNum);
            if (res != 0)
            {
                return run_time_error(NULL);
            }
            break;
        }
        case OP_METHOD_CALL:
        {
            StringObject *name = (StringObject *)(consts[OP_VALUE].u.object);
            int argsNum = (int)(popStack()).u.number;
            int res = methodCall(name, argsNum);
            if (res != 0)
            {
                return run_time_error(NULL);
            }
            break;
        }
        case OP_RETURN:
        {
            Value rtn = popStack();
            vm.topStack = start;
            pushStack(rtn);
            return 0;
        }
        case OP_LOAD_ATTR:
        {
            StringObject *name = (StringObject *)(consts[OP_VALUE].u.object);
            Value val = vm.stack[vm.topStack - 1];
            if (val.type == SUPER)
            {
                InstanceObject *ins = (InstanceObject *)val.u.object;
                if (ins->inClass->super == NULL)
                {
                    return run_time_error("super is null");
                }
                ClassObject *co = ins->inClass->super;
                HashNode *node = getHashMap(&co->fields, name);
                if (node == NULL)
                {
                    return run_time_error("can not load attribute");
                }
                load_attr_obj(node->v);
            }
            else if (val.type == OBJECT && val.u.object->type == INSTANCE_OBJ)
            {
                InstanceObject *ins = (InstanceObject *)val.u.object;
                HashNode *node = getHashMap(&ins->fields, name);
                if (node == NULL)
                {
                    node = getHashMap(&ins->inClass->fields, name);
                }
                if (node == NULL)
                {
                    return run_time_error("can not load attribute");
                }
                load_attr_obj(node->v);
            }
            else if (val.type == OBJECT && val.u.object->type == MAP_OBJ)
            {
                MapObject *map = (MapObject *)val.u.object;
                HashNode *node = getHashMap(&map->map, name);
                if (node == NULL)
                {
                    return run_time_error("can not load attribute");
                }
                vm.stack[vm.topStack - 1] = node->v;
            }
            else
            {
                return run_time_error("can not load attribute");
            }
            break;
        }
        case OP_STORE_ATTR:
        {
            StringObject *name = (StringObject *)(consts[OP_VALUE].u.object);
            Value callee = popStack();
            Value caller = popStack();
            if (caller.type != OBJECT)
            {
                return run_time_error("can not store attribute");
            }
            switch (caller.u.object->type)
            {
            case INSTANCE_OBJ:
            {
                InstanceObject *ins = (InstanceObject *)caller.u.object;
                saveHashNode(&ins->fields, name, callee);
                break;
            }
            case MAP_OBJ:
            {
                MapObject *map = (MapObject *)caller.u.object;
                saveHashNode(&map->map, name, callee);
                break;
            }
            default:
            {
                return run_time_error("can not store attribute");
            }
            }
            break;
        }
        case OP_THIS:
        {
            Value val = vm.stack[start];
            if (val.type != OBJECT && val.u.object->type != INSTANCE_OBJ)
            {
                return run_time_error("this type error");
            }
            pushStack(val);
            break;
        }
        case OP_SUPER:
        {
            Value val = vm.stack[start];
            if (val.type != OBJECT && val.u.object->type != INSTANCE_OBJ)
            {
                return run_time_error("super type error");
            }
            val.type = SUPER;
            pushStack(val);
            break;
        }
        case OP_PLUS:
        {
            Value r = vm.stack[vm.topStack - 1]; /*因该操作可能会引起gc回收，防止被gc回收不用pop操作*/
            Value l = vm.stack[vm.topStack - 2];
            switch (l.type)
            {
            case NUMBER:
            {
                if (r.type != NUMBER)
                {
                    return run_time_error("an error occurred in +");
                }
                vm.stack[vm.topStack - 2] = num_to_val(l.u.number + r.u.number);
                break;
            }
            case OBJECT:
            {
                if (l.u.object->type != r.u.object->type)
                {
                    return run_time_error("an error occurred in +");
                }
                if (l.u.object->type == STRING_OBJ)
                {
                    StringObject *str = concatString(l, r);
                    vm.stack[vm.topStack - 2] = obj_to_val(str);
                }
                else if (l.u.object->type == LIST_OBJ)
                {
                    ListObject *lst = concatList(l, r);
                    vm.stack[vm.topStack - 2] = obj_to_val(lst);
                }
                else
                {
                    return run_time_error("an error occurred in +");
                }
                break;
            }
            default:
            {
                return run_time_error("an error occurred in +");
            }
            }
            vm.topStack--;
            break;
        }
        case OP_SUB:
        {
            Value r = popStack();
            Value l = popStack();
            exp_is_number(l, r);
            pushStack(num_to_val(l.u.number - r.u.number));
            break;
        }
        case OP_MUL:
        {
            Value r = popStack();
            Value l = popStack();
            exp_is_number(l, r);
            pushStack(num_to_val(l.u.number * r.u.number));
            break;
        }
        case OP_DIV:
        {
            Value r = popStack();
            Value l = popStack();
            exp_is_number(l, r);
            if (r.u.number == 0)
            {
                return run_time_error("divide zero");
            }
            pushStack(num_to_val(l.u.number / r.u.number));
            break;
        }
        case OP_EQ:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            if (exp_is_bool(l, r))
            {
                val.type = (l.type == r.type) ? TRUE : FALSE;
            }
            else
            {
                if (l.type != r.type)
                {
                    return run_time_error("an error occurred in option");
                }
                switch (l.type)
                {
                case NUMBER:
                {
                    val.type = (l.u.number == r.u.number) ? TRUE : FALSE;
                    break;
                }
                case OBJECT:
                {
                    val.type = (l.u.object == r.u.object) ? TRUE : FALSE;
                    break;
                }
                default:
                {
                    return run_time_error("an error occurred in option");
                }
                }
            }
            pushStack(val);
            break;
        }
        case OP_NOT_EQ:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            if (exp_is_bool(l, r))
            {
                val.type = (l.type == r.type) ? FALSE : TRUE;
            }
            else
            {
                if (l.type != r.type)
                {
                    return run_time_error("an error occurred in option");
                }
                switch (l.type)
                {
                case NUMBER:
                {
                    val.type = (l.u.number == r.u.number) ? FALSE : TRUE;
                    break;
                }
                case OBJECT:
                {
                    val.type = (l.u.object == r.u.object) ? FALSE : TRUE;
                    break;
                }
                default:
                {
                    return run_time_error("an error occurred in option");
                }
                }
            }
            pushStack(val);
            break;
        }
        case OP_OR:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            if (exp_is_bool(l, r))
            {
                val = (l.type == FALSE) ? r : l;
            }
            else
            {
                exp_is_number(l, r);
                val = (l.u.number == 0.0) ? r : l;
            }
            pushStack(val);
            break;
        }
        case OP_AND:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            if (exp_is_bool(l, r))
            {
                val = (l.type == FALSE) ? l : r;
            }
            else
            {
                exp_is_number(l, r);
                val = (l.u.number == 0.0) ? l : r;
            }
            pushStack(val);
            break;
        }
        case OP_LT_EQ:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            exp_is_number(l, r);
            val.type = l.u.number <= r.u.number ? TRUE : FALSE;
            pushStack(val);
            break;
        }
        case OP_GT_EQ:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            exp_is_number(l, r);
            val.type = l.u.number >= r.u.number ? TRUE : FALSE;
            pushStack(val);
            break;
        }
        case OP_GT:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            exp_is_number(l, r);
            val.type = l.u.number > r.u.number ? TRUE : FALSE;
            pushStack(val);
            break;
        }
        case OP_LT:
        {
            Value r = popStack();
            Value l = popStack();
            Value val;
            exp_is_number(l, r);
            val.type = l.u.number < r.u.number ? TRUE : FALSE;
            pushStack(val);
            break;
        }
        case OP_NOT:
        {
            Value val = popStack();
            switch (val.type)
            {
            case FALSE:
                val.type = TRUE;
                break;
            case TRUE:
                val.type = FALSE;
                break;
            default:
            {
                return run_time_error("an error occurred in option");
            }
            }
            pushStack(val);
            break;
        }
        case OP_MINUS:
        {
            Value val = popStack();
            if (val.type != NUMBER)
            {
                return run_time_error("an error occurred in option");
            }
            val.u.number = -val.u.number;
            pushStack(val);
            break;
        }
        case OP_GOTO_IF_FALSE:
        {
            int pos = OP_VALUE;
            Value val = popStack();
            switch (val.type)
            {
            case FALSE:
                ip = pos - 1;
                break;
            case TRUE:
                break;
            default:
            {
                return run_time_error("type error");
            }
            }
            break;
        }
        case OP_GOTO:
        {
            int pos = OP_VALUE;
            ip = pos - 1;
            break;
        }
        case OP_INDEX:
        {
            Value idx = popStack();
            Value vec = popStack();
            if (vec.type != OBJECT)
            {
                return run_time_error("type error");
            }
            Value val;
            MyObject *obj = vec.u.object;
            switch (obj->type)
            {
            case LIST_OBJ:
            {
                VecValue *lst = &((ListObject *)obj)->list;
                if (idx.type != NUMBER && (int)idx.u.number != idx.u.number)
                {
                    return run_time_error("index must integer type");
                }
                int pos = (int)idx.u.number;
                if (pos > lst->n || pos < 0)
                {
                    return run_time_error("index length error");
                }
                val = lst->data[pos];
                break;
            }
            case MAP_OBJ:
            {
                HashMap *map = &((MapObject *)obj)->map;
                if (idx.type != OBJECT && idx.u.object->type != STRING_OBJ)
                {
                    return run_time_error("index must string type");
                }
                StringObject *key = (StringObject *)idx.u.object;
                HashNode *node = getHashMap(map, key);
                if (node == NULL)
                {
                    return run_time_error("can not find key in map");
                }
                val = node->v;
                break;
            }
            default:
            {
                return run_time_error("type error");
            }
            }
            pushStack(val);
            break;
        }
        case OP_STORE_INDEX:
        {
            Value val = popStack();
            Value idx = popStack();
            Value vec = popStack();
            if (vec.type != OBJECT)
            {
                return run_time_error("type error");
            }
            MyObject *obj = vec.u.object;
            switch (obj->type)
            {
            case LIST_OBJ:
            {
                VecValue *lst = &((ListObject *)obj)->list;
                if (idx.type != NUMBER && (int)idx.u.number != idx.u.number)
                {
                    return run_time_error("index must integer type");
                }
                int pos = (int)idx.u.number;
                if (pos > lst->n || pos < 0)
                {
                    return run_time_error("index length error");
                }
                lst->data[pos] = val;
                break;
            }
            case MAP_OBJ:
            {
                HashMap *map = &((MapObject *)obj)->map;
                if (idx.type != OBJECT && idx.u.object->type != STRING_OBJ)
                {
                    return run_time_error("index must string type");
                }
                StringObject *key = (StringObject *)idx.u.object;
                saveHashNode(map, key, val);
                break;
            }
            default:
            {
                return run_time_error("type error");
            }
            }
            break;
        }
        case OP_ARRAY_LENGTH:
        {
            Value val = popStack();
            double len = (double)(((ListObject *)val.u.object)->list.n);
            pushStack(num_to_val(len));
            break;
        }
        default:
            printf("unreachable");
            exit(1);
        };
        ip++;
    }
}
/********************
 * 以下是内存管理部分
 ********************
 */
void markValue(Value v);
void markObject(MyObject *obj);
/*
 * markHashMap(HashMap *map) - 标记哈希map
 * @map：哈希map
 *
 * Return:void
 */
void markHashMap(HashMap *map)
{
    int i;
    HashNode *tmp = NULL;
    for (i = 0; i < map->cap; i++)
    {
        for (tmp = map->hashNode[i]; tmp != NULL; tmp = tmp->next)
        {
            markObject((MyObject *)tmp->string);
            markValue(tmp->v);
        }
    }
}
/*
 * markVecValue(VecValue *vec) - 标记值数组
 * @vec：值数组
 *
 * Return:void
 */
void markVecValue(VecValue *vec)
{
    int i;
    for (i = 0; i < vec->n; i++)
    {
        markValue(vec->data[i]);
    }
}
/*
 * markObject(MyObject *obj) - 标记对象
 * @obj：对象
 *
 * 如果对象是空，或者对象已经被标记，或者对象是99的直接返回
 * 99类型是空闲列表中的对象
 *
 * Return:void
 */
void markObject(MyObject *obj)
{
    if (obj == NULL || obj->mark == 1 || obj->mark == 99)
    {
        return;
    }
    obj->mark = 1;
    switch (obj->type)
    {
    case LIST_OBJ:
    {
        ListObject *o = (ListObject *)obj;
        markVecValue(&o->list);
        break;
    }
    case MAP_OBJ:
    {
        MapObject *o = (MapObject *)obj;
        markHashMap(&o->map);
        break;
    }
    case INSTANCE_OBJ:
    {
        InstanceObject *o = (InstanceObject *)obj;
        markHashMap(&o->fields);
        markObject((MyObject *)o->inClass);
        break;
    }
    case CLASS_OBJ:
    {
        ClassObject *o = (ClassObject *)obj;
        markHashMap(&o->fields);
        markObject((MyObject *)o->super);
        break;
    }
    case MEMBER_OBJ:
    {
        MemberObject *o = (MemberObject *)obj;
        markObject(o->callee);
        markObject(o->callee);
        break;
    }
    default:
        break;
    }
}
/*
 * markValue(Value v) - 标记值
 * @obj：值
 *
 * Return:void
 */
void markValue(Value v)
{
    if (v.type == OBJECT)
    {
        markObject(v.u.object);
    }
}
/*
 * gcMark() - 标记在用的全部对象
 *
 * 该函数会查找当前时刻所有在使用的对象并标记
 * 对运行栈的中的对象进行标记
 * 对全局变量中的对象进行标记
 * 对创建的字符串进行标记
 *
 * Return:void
 */
void gcMark()
{
    int i;
    for (i = 0; i < vm.topStack; i++)
    {
        markValue(vm.stack[i]);
    }
    markHashMap(&vm.globals);
    markObject((MyObject *)vm.constructorStr);
}
/*
 * freeHashMap(HashMap *map) - 释放哈希map
 *
 * 该函数会查找哈希map中每一个节点，并把每一个节点加入vm.gc.nodeLink这个空闲列表中
 * 最后释放map->hashNode内存空间
 *
 * Return:void
 */
void freeHashMap(HashMap *map)
{
    int i;
    HashNode *curr, *tmp;
    for (i = 0; i < map->cap; i++)
    {
        curr = map->hashNode[i];
        while (curr != NULL)
        {
            tmp = curr;
            tmp->string = NULL;
            tmp->v = (Value){NONE};
            curr = curr->next;
            tmp->next = vm.gc.nodeLink;
            vm.gc.nodeLink = tmp;
        }
    }
    free(map->hashNode);
    map->hashNode = NULL;
    map->cap = 0;
}
/*
 * freeObj(MyObject *obj) - 释放对象
 *
 * 对象如果是空直接返回，如果不为空给对象的mark标识赋值99，即表示在空闲列表中
 * 然后根据对象的类型进入不同的分支，把由对象创建的内存释放，并把对象加入空闲列表中
 *
 * Return:void
 */
void freeObj(MyObject *obj)
{
    if (obj == NULL)
    {
        return;
    }
    obj->mark = 99;
    switch (obj->type)
    {
    case STRING_OBJ:
    {
        StringObject *o = (StringObject *)obj;
        free(o->str);
        o->str = NULL;
        o->next = vm.gc.stringLink;
        vm.gc.stringLink = (MyObject *)o;
        break;
    }
    case LIST_OBJ:
    {
        ListObject *o = (ListObject *)obj;
        free(o->list.data);
        o->list.data = NULL;
        o->next = vm.gc.listLink;
        vm.gc.listLink = (MyObject *)o;
        break;
    }
    case MAP_OBJ:
    {
        MapObject *o = (MapObject *)obj;
        freeHashMap(&o->map);
        o->next = vm.gc.mapLink;
        vm.gc.mapLink = (MyObject *)o;
        break;
    }
    case INSTANCE_OBJ:
    {
        InstanceObject *o = (InstanceObject *)obj;
        freeHashMap(&o->fields);
        o->next = vm.gc.insLink;
        vm.gc.insLink = (MyObject *)o;
        break;
    }
    case CLASS_OBJ:
    {
        ClassObject *o = (ClassObject *)obj;
        freeHashMap(&o->fields);
        o->next = vm.gc.classLink;
        vm.gc.classLink = (MyObject *)o;
        break;
    }
    case MEMBER_OBJ:
    {
        MemberObject *o = (MemberObject *)obj;
        o->callee = NULL;
        o->caller = NULL;
        o->next = vm.gc.memLink;
        vm.gc.memLink = (MyObject *)o;
        break;
    }
    case BUILDIN_OBJ:
    {
        BuildinObject *o = (BuildinObject *)obj;
        free(o);
        break;
    }
    default:
        printf("unreachable");
        exit(1);
    }
    obj = NULL;
}
/*
 * gcSweep() - 在vm.gc.head链表中清除未使用的对象
 *
 * 循环查找vm.gc.head，如果是对象的mark是0说明该对象没有被使用可以清除
 * 如果mark是1说明对象在使用，把mark置成0
 *
 * Return:void
 */
void gcSweep()
{
    MyObject *prev, *tmp, *curr;
    curr = vm.gc.head, tmp = curr, prev = curr;
    while (curr != NULL)
    {
        if (curr->mark == 0)
        {
            tmp = curr;
            prev = prev->next;
            curr = curr->next;
            freeObj(tmp);
        }
        else
        {
            curr->mark = 0;
            curr = curr->next;
            break;
        }
    }
    while (curr != NULL)
    {
        if (curr->mark == 1)
        {
            curr->mark = 0;
            prev = curr;
            curr = curr->next;
        }
        else
        {
            tmp = curr;
            curr = curr->next;
            prev->next = curr;
            freeObj(tmp);
        }
    }
}
/*
 * gcStart() - 标记清除垃圾回收
 *
 * gcMark() 对在使用的对象进行标记
 * gcSweep() 对没有标记到的对象进行清除
 *
 * Return:void
 */
void gcStart()
{
    gcMark();
    gcSweep();
}
/*
 * gcMalloc(int size) - 分配对象
 * @size:对象大小
 *
 * 判断当前对象是否超过对象的容量，如果超过把对象的容量扩充2倍，启动gc回收
 * 然后分配内存，使用init_object_head宏把对象加入对象链表中
 *
 * Return:分配的对象
 */
MyObject *gcMalloc(int size)
{
    if (vm.gc.n >= vm.gc.cap)
    {
        vm.gc.cap = vm.gc.cap * 2;
        gcStart();
    }
    MyObject *obj = (MyObject *)malloc(size);
    if (obj == NULL)
    {
        printf("not enough memory");
        exit(1);
    }
    vm.gc.n++;
    init_object_head(obj);
    return obj;
}
/*
 * freeLexer() - 释放词法分析器
 *
 * Return:void
 */
void freeLexer()
{
    free(lex.content);
    int i;
    StringObject *curr, *tmp;
    for (i = 0; i < lex.strMap.cap; i++)
    {
        curr = lex.strMap.tabStr[i];
        while (curr != NULL)
        {
            tmp = curr;
            curr = (StringObject *)curr->next;
            free(tmp->str);
            free(tmp);
        }
    }
}
/*
 * freeCompiler(MyCompiler *c) - 释放编译环境
 * @c:编译环境
 *
 * Return:void
 */
void freeCompiler(MyCompiler *c)
{
    free(c->consts.data);
    free(c->inCode.data);
    free(c->lines.data);
    free(c->var.data);
    free(c);
}
/*
 * freeParser() - 释放语法分析器
 *
 * Return:void
 */
void freeParser()
{
    MyCompiler *tmp, *curr;
    curr = parser.head;
    while (curr != NULL)
    {
        tmp = curr;
        curr = (MyCompiler *)curr->next;
        freeCompiler(tmp);
    }
    free(parser.tabElse.data);
    free(parser.tabBreak.data);
    free(parser.tabFor.data);
    free(parser.tabLine.data);
}

#define loop_free(T, F, link)  \
    do                         \
    {                          \
        T *tmp, *curr;         \
        curr = link;           \
        while (curr != NULL)   \
        {                      \
            tmp = curr;        \
            curr = curr->next; \
            F(tmp);            \
            tmp = NULL;        \
        }                      \
    } while (0)

/*
 * leave() - 离开时释放所有
 *
 * 循环释放对象链表vm.gc.head
 * 循环释放所有空闲列表
 * 释放词法分析器
 * 释放语法分析器
 *
 * Return:void
 */
void leave()
{
    loop_free(MyObject, freeObj, vm.gc.head);
    loop_free(MyObject, free, vm.gc.stringLink);
    loop_free(MyObject, free, vm.gc.listLink);
    loop_free(MyObject, free, vm.gc.mapLink);
    loop_free(MyObject, free, vm.gc.insLink);
    loop_free(MyObject, free, vm.gc.memLink);
    loop_free(MyObject, free, vm.gc.classLink);
    loop_free(HashNode, free, vm.gc.nodeLink);
    freeLexer();
    freeParser();
}

/*********************
 * 以下是标准库实现部分
 *********************
 */
int printValue(Value v, int flag);
/*
 * printMap(HashMap *map) - 打印哈希map
 * @map：哈希map
 *
 * Return:void
 */
void printMap(HashMap *map)
{
    printf("{");
    int n = 0;
    int i;
    HashNode *tmp;
    for (i = 0; i < map->cap; i++)
    {
        tmp = map->hashNode[i];
        while (tmp != NULL)
        {
            printf("'%s':", tmp->string->str);
            printValue(tmp->v, 1);
            tmp = tmp->next;
            n++;
            if (n < map->n)
            {
                printf(",");
            }
        }
    }
    printf("}");
}
/*
 * printObject(MyObject *obj) - 打印对象
 * @obj:对象
 *
 * Return:void
 */
void printObject(MyObject *obj)
{
    switch (obj->type)
    {
    case STRING_OBJ:
        printf("%s", ((StringObject *)obj)->str);
        break;
    case LIST_OBJ:
    {
        VecValue vec = ((ListObject *)obj)->list;
        printf("[");
        int i;
        for (i = 0; i < vec.n; i++)
        {
            printValue(vec.data[i], 1);
            if (i + 1 != vec.n)
            {
                printf(",");
            }
        }
        printf("]");
        break;
    }
    case MAP_OBJ:
    {
        printMap(&((MapObject *)obj)->map);
        break;
    }
    case INSTANCE_OBJ:
    {
        printf("<instance>");
        break;
    }
    case COMPILER_OBJ:
    {
        printf("<compiler>:%s", ((MyCompiler *)obj)->name->str);
        break;
    }
    case CLASS_OBJ:
    {
        printf("<class>:%s", ((ClassObject *)obj)->name->str);
        break;
    }
    case MEMBER_OBJ:
    {
        printf("<member>");
        break;
    }
    case BUILDIN_OBJ:
    {
        printf("<buildIn>");
        break;
    }
    default:
    {
        printf("unreachable");
        exit(1);
    }
    }
}
/*
 * printValue(Value v,int flag) - 打印值
 * @v:值
 * @flag:是否打印object内容
 *
 * Return:void
 */
int printValue(Value v, int flag)
{
    switch (v.type)
    {
    case NUMBER:
        printf("%g", v.u.number);
        break;
    case TRUE:
        printf("true");
        break;
    case FALSE:
        printf("false");
        break;
    case NONE:
        printf("none");
        break;
    case SUPER:
        printf("super");
        break;
    case OBJECT:
    {
        if (flag == 0)
        {
            printObject(v.u.object);
        }
        else
        {
            printf("<OBJECT>");
        }
        break;
    }
    default:
        printf("unreachable");
        exit(1);
    }
    return 0;
}
/*
 * stdPrint(int argsNum, Value *args) - 标准库打印多值
 * @argsNum:参数数量
 * @args:参数值
 *
 * 打印参数中的内容
 *
 * Return:错误标识
 */
int stdPrint(int argsNum, Value *args)
{
    int i;
    for (i = 0; i < argsNum; i++)
    {
        if (printValue(args[i + 1], 0) == 1)
        {
            return 1;
        }
        printf(" ");
    }
    printf("\n");
    args[0] = (Value){NONE};
    return 0;
}
int stdNow(int argsNum, Value *args)
{
    if (argsNum > 0)
    {
        return setError("argument error");
    }
    double t = (double)clock();
    args[0] = num_to_val(t);
    return 0;
}
int stdRandom(int argsNum, Value *args)
{
    if (argsNum > 0)
    {
        return setError("argument error");
    }
    srand((unsigned int)time(NULL) + (unsigned int)rand());
    double r = (double)rand() / (double)RAND_MAX;
    args[0] = num_to_val(r);
    return 0;
}
int stdString(int argsNum, Value *args)
{
    if (argsNum != 1)
    {
        return setError("argument error");
    }
    Value v = args[1];
    switch (v.type)
    {
    case NUMBER:
    {
        char str[24];
        sprintf(str, "%.14g", v.u.number);
        StringObject *s = initStringObject(utilStrdup(str));
        args[0] = obj_to_val(s);
        break;
    }
    case OBJECT:
    {
        MyObject *obj = v.u.object;
        switch (obj->type)
        {
        case STRING_OBJ:
            args[0] = v;
            break;
        default:
            return setError("type can not convert to a string");
        }
        break;
    }
    default:
        return setError("type can not convert to a string");
    }
    return 0;
}
int stdStringCharAt(int argsNum, Value *args)
{
    if (argsNum != 1)
    {
        return setError("argument error");
    }
    Value v = args[1];
    switch (v.type)
    {
    case NUMBER:
    {
        int pos = (int)v.u.number;
        StringObject *str = (StringObject *)args[0].u.object;
        if (pos > str->len || pos < 0)
        {
            return setError("argument index error");
        }
        char c = str->str[pos];
        StringObject *s = initStringObject(utilStrdup(&c));
        args[0] = obj_to_val(s);
        break;
    }
    default:
        return setError("charAt argumnet must number");
    }
    return 0;
}
int stdStringSubstr(int argsNum, Value *args)
{
    if (argsNum != 2)
    {
        return setError("argument error");
    }
    Value from = args[1];
    Value to = args[2];
    if (from.type == NUMBER && to.type == NUMBER)
    {
        int start = (int)from.u.number;
        int end = (int)to.u.number;
        StringObject *str = (StringObject *)args[0].u.object;
        if (start > str->len || start < 0 || end < 0 || start + end > str->len)
        {
            return setError("argument index error");
        }
        char *c = malloc(str->len + 1);
        memcpy(c, str->str + start, str->len);
        StringObject *s = initStringObject(c);
        args[0] = obj_to_val(s);
    }
    else
    {
        return setError("substr argumnet must number");
    }
    return 0;
}
int stdListPush(int argsNum, Value *args)
{
    if (argsNum != 1)
    {
        return setError("argument error");
    }
    Value val = args[1];
    ListObject *lst = (ListObject *)args[0].u.object;
    pushValueVec(&lst->list, val);
    args[0] = (Value){NONE};
    return 0;
}
int stdListPop(int argsNum, Value *args)
{
    if (argsNum != 0)
    {
        return setError("argument error");
    }
    ListObject *lst = (ListObject *)args[0].u.object;
    Value val = popValueVec(&lst->list);
    args[0] = val;
    return 0;
}
int stdMapGet(int argsNum, Value *args)
{
    if (argsNum != 1)
    {
        return setError("argument error");
    }
    Value idx = args[1];
    if (idx.type != OBJECT && idx.u.object->type != STRING_OBJ)
    {
        return setError("key must string type");
    }
    StringObject *key = (StringObject *)idx.u.object;
    MapObject *map = (MapObject *)args[0].u.object;
    HashNode *node = getHashMap(&map->map, key);
    if (node == NULL)
    {
        return setError("can not find key in map");
    }
    args[0] = node->v;
    return 0;
}
int stdMapSet(int argsNum, Value *args)
{
    if (argsNum != 2)
    {
        return setError("argument error");
    }
    Value idx = args[1];
    Value val = args[2];
    if (idx.type != OBJECT && idx.u.object->type != STRING_OBJ)
    {
        return setError("key must string type");
    }
    StringObject *key = (StringObject *)idx.u.object;
    MapObject *map = (MapObject *)args[0].u.object;
    saveHashNode(&map->map, key, val);
    args[0] = (Value){NONE};
    return 0;
}
/*
 * stdlibBuiltin(HashMap *map, char *name, void *func) - 内建函数
 * @map:哈希map
 * @name:内建函数名
 * @func:函数指针
 *
 * 把内建函数加入到哈希map中
 * 这里使用initStringNoGc创建字符串的原因是用gcMalloc会触发垃圾回收
 *
 * Return:void
 */
void stdlibBuiltin(HashMap *map, char *name, void *func)
{
    BuildinObject *obj = (BuildinObject *)gcMalloc(sizeof(BuildinObject));
    obj->type = BUILDIN_OBJ;
    obj->inFunc = func;
    StringObject *str = initStringNoGc(utilStrdup(name));
    init_object_head(str);
    saveHashNode(map, str, obj_to_val(obj));
}
/*
 * stdAddClass(char *name) - 内建class对象
 * @name：class对象名称
 *
 * 把内建class对象加入到全局变量中
 *
 * Return:class对象
 */
ClassObject *stdAddClass(char *name)
{
    StringObject *str = initStringNoGc(utilStrdup(name));
    ClassObject *cls = initClass(str, NULL, 0);
    init_object_head(str);
    saveHashNode(&vm.globals, str, obj_to_val(cls));
    return cls;
}
/*
 * stdAddInstance(ClassObject *cls, char *name) - 内建实例对象
 * @cls：class对象
 * @name:实例对象名称
 *
 * 把内建实例对象加入到全局变量中
 *
 * Return:void
 */
void stdAddInstance(ClassObject *cls, char *name)
{
    StringObject *str = initStringNoGc(utilStrdup(name));
    InstanceObject *ins = initInstanceObject(cls);
    init_object_head(str);
    saveHashNode(&vm.globals, str, obj_to_val(ins));
}
/*
 * stdlibConsole() - 内建Console类及console实例
 *
 * 使用stdAddClass创建Console类
 * 把内建函数log加入到类成员中
 * 使用stdAddInstance创建console实例对象
 *
 * Return:void
 */
void stdlibConsole()
{
    ClassObject *cls = stdAddClass("Console");
    stdlibBuiltin(&cls->fields, "log", stdPrint);
    stdAddInstance(cls, "console");
}
/*
 * registerStdlib() - 注册标准库到全局变量中
 *
 * Return:void
 */
void registerStdlib()
{
    stdlibBuiltin(&vm.globals, "print", stdPrint);
    stdlibBuiltin(&vm.globals, "now", stdNow);
    stdlibBuiltin(&vm.globals, "random", stdRandom);
    stdlibBuiltin(&vm.globals, "toString", stdString);
    stdlibBuiltin(&vm.stringClass->fields, "charAt", stdStringCharAt);
    stdlibBuiltin(&vm.stringClass->fields, "substr", stdStringSubstr);
    stdlibBuiltin(&vm.listClass->fields, "push", stdListPush);
    stdlibBuiltin(&vm.listClass->fields, "pop", stdListPop);
    stdlibBuiltin(&vm.mapClass->fields, "get", stdMapGet);
    stdlibBuiltin(&vm.mapClass->fields, "set", stdMapSet);
    stdlibConsole();
}
int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        printf("please enter a file name\n");
        return 1;
    }
    StringObject *name = initLexer(argv[1]);
    MyCompiler *top = initCompiler(name);
    initParser(top);
    startParser();
#ifdef DEBUG
    showParser(top);
#endif
    initVm(top);
    int isErr = startVm(top, 0);
    leave();
    return isErr;
}
