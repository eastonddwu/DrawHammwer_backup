# UA后台编码规范

ua后台编码规范的原则是精简，易读、易记，不做太多约束。

### history

* 2019-06-25 1.0
* 2019-08-06 1.01
* 2020-04-04 1.1 增加了auto和lambda的使用规范
* 2021-07-13 1.1.01 增加补充建议

### 总述
函数命名, 变量命名, 文件命名要有描述性; 少用缩写.

### 说明
尽可能使用描述性的命名, 别心疼空间, 毕竟相比之下让代码易于新读者理解更重要. 不要用只有项目开发者能理解的缩写, 也不要通过砍掉几个字母来缩写单词.

```
int price_count_reader;    // 无缩写
int num_errors;            // "num" 是一个常见的写法
int num_dns_connections;   // 人人都知道 "DNS" 是什么

int n;                     // 毫无意义.
int nerr;                  // 含糊不清的缩写.
int n_comp_conns;          // 含糊不清的缩写.
int wgc_connections;       // 只有贵团队知道是什么意思.
int pc_reader;             // "pc" 有太多可能的解释了.
int cstmr_id;              // 删减了若干字母.
```

注意, 一些特定的广为人知的缩写是允许的, 例如用 i 表示迭代变量和用 T 表示模板参数.

模板参数的命名应当遵循对应的分类: 类型模板参数应当遵循 类型命名 的规则, 而非类型模板应当遵循 变量命名 的规则.

## 文件编码

所有文件编码**必须**用utf-8，用visual studio的必须自行指定

## class命名
### 总述

class名称的每个单词首字母均大写, 不包含下划线: MyExcitingClass, MyExcitingEnum.

### 说明

所有类型命名 —— class, struct, typedef, enum, 类型模板参数 —— 均使用相同约定, 即以大写字母开始, 每个单词首字母均大写, 不包含下划线. 例如:

```
// 类和结构体
class UrlTable { ...
class UrlTableTester { ...
struct UrlTableProperties { ...

// 类型定义
typedef hash_map<UrlTableProperties *, string> PropertiesMap;

// using 别名
using PropertiesMap = hash_map<UrlTableProperties *, string>;

// 枚举
enum UrlTableErrors { ...
```

## 变量命名
### 总述

变量 (包括函数参数) 和数据成员名一律小写, 单词之间用下划线连接. 类的成员变量以下划线结尾, 但结构体的就不用, 如: ```a_local_variable, a_struct_data_member, a_class_data_member_```.

### 普通变量命名

```
string table_name;  // 好 - 用下划线.
string tablename;   // 好 - 全小写.

string tableName;  // 不合规范 - 混合大小写
```

### 类数据成员

不管是静态的还是非静态的, 类数据成员都可以和普通变量一样, 但要接下划线.
```
class TableInfo
{
    ...
private:
    string table_name_;  // 好 - 后加下划线.
    string tablename_;   // 好.
    static Pool<TableInfo>* pool_;  // 好.
};
```

### 结构体变量

不管是静态的还是非静态的, 结构体数据成员都可以和普通变量一样, 不用像类那样接下划线:
```
struct UrlTableProperties
{
    string name;
    int num_entries;
    static Pool<UrlTableProperties>* pool;
};
```
## 常量命名
声明为 constexpr 或 const 的变量, 或在程序运行期间其值始终保持不变的, 命名时以 “k” 开头, 大小写混合. 例如:

```const int kDaysInAWeek = 7;```

或全部使用大写字母，例如：

```
enum MyEnum
{
	FIRST = 1,
	SECOND,
	THRID
}
```
## 函数命名
无论是成员函数、还是全局函数，全部采用大写字母开头、大小写混合的命名方式，以作为与STL函数的区分。例如：
```
void MyFunction(...)

class MyClass
{
...
   void MyMethod() ...
};
```

## auto使用规范
auto用得太多，会导致代码的可读性变差。

首先，在调用函数、成员函数时，明确不允许使用auto。例如

```
auto a = f(); //  decltype(a) = ?
auto b = my_object.method(....); // 可读性很差，影响代码阅读
```

只允许在以下5中情况下使用auto。

* 将Lambda表达式和变量绑定的时候可以使用，这是因为Lambda不能显式的声明类型名

```
auto f = [](){};
```

* 用作迭代器变量的时候可以使用，但只能用在当迭代器的类型名称很冗长，影响到代码可读性的时候

```
// 迭代器用法
for(auto it = xxx.begin(); it != xxx.end; ++it) { ... }

// 不需要修改元素的range based for，明确声明为const auto&
for(const auto& item: container) { ... }

// 需要修改元素的range based for，声明为auto&&或auto&
for(auto&& item: container) { ... }
```

* 在模板代码中使用，尤其是当表达式的类型不太容易辨别的时候
不举例，ua模板代码少。

* 在显示调用构造函数创建对象的时候。

```
// 此处的auto一看就知道是啥类型，且very_very_long_class_name不用写两次
auto p = new very_very_long_class_name(blabla...);
// 静态工厂函数、或singleton
auto inst = GlobalTimerManager::GetInstance(); // 一看就是GlobalTimerManager类型，可以用auto
```
* 结构化绑定(Structured binding declaration in C++17)

```
auto [it, secceeded] = some_map.insert(....);
for(auto&& [key, value]: some_map) { ... }
```

## 补充建议

### 使用clang-format

&#160; &#160; &#160; &#160; 项目中已经添加了代码风格约束的`.clang-format`，所以需要自行安装`clang-format`以获得更好的支持。对于`VS Code`用户来说，在安装`clang-format`之后，其插件安装目录下会有最新的`clang-format`可执行程序，你需要的只是把它加入到环境的可执行目录中以保证Linux环境下全局可访问执行。对于非vsc用户，可以直接安装clang-format（这里无需完整安装LLVM -.-||)，需要注意的可能是一些`.so`动态链接可能会报错，需要注意下版本并下载安装即可。

### 使用space替换tab

&#160; &#160; &#160; &#160; 项目使用4字符对齐(.clang-format中已指定)，这里建议使用空格而不是tab来填充，不然编码不统一，不仅很丑，而且可能带来未知问题(Python笑了)。

### 使用换行慷慨些

&#160; &#160; &#160; &#160; 对于一段逻辑，全部编码在一起，很容造成视觉冲击。一段冗长的代码很容易让人喘不过气(压迫感很强)。建议多使用换行来让一段逻辑的各个执行流程更加清晰(多读读他人的代码大有收益)。
