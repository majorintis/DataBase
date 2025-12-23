#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <cctype>

// 定义支持的数据类型
enum class DataType {
    INT,
    STRING
};

// 数据值类（存储任意支持的类型值）
class Value {
private:
    DataType type;
    int int_val;
    std::string str_val;

public:
    // 新增：默认构造函数（解决pair和map的默认构造问题）
    Value() : type(DataType::INT), int_val(0), str_val("") {}
    Value(int val) : type(DataType::INT), int_val(val), str_val("") {}
    Value(const std::string& val) : type(DataType::STRING), int_val(0), str_val(val) {}

    DataType get_type() const { return type; }
    int get_int() const {
        if (type != DataType::INT) {
            throw std::runtime_error("Value is not an integer");
        }
        return int_val;
    }
    std::string get_string() const {
        if (type != DataType::STRING) {
            throw std::runtime_error("Value is not a string");
        }
        return str_val;
    }

    // 重载比较运算符（用于WHERE条件）
    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return int_val == other.int_val;
        return str_val == other.str_val;
    }

    bool operator!=(const Value& other) const {
        return !(*this == other);
    }

    // 转换为字符串用于输出
    std::string to_string() const {
        if (type == DataType::INT) {
            return std::to_string(int_val);
        }
        return str_val;
    }
};

// 工具函数：去除字符串首尾空格
std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

// 工具函数：提取括号内的内容（如 (a,b,c) 提取为 a,b,c）
std::string extract_bracketed_content(const std::string& s, char open = '(', char close = ')') {
    size_t start = s.find(open);
    size_t end = s.find(close, start + 1);
    if (start == std::string::npos || end == std::string::npos) {
        throw std::runtime_error("Missing brackets in SQL statement");
    }
    return s.substr(start + 1, end - start - 1);
}

// 工具函数：按逗号分割字符串并去除每个元素的空格
std::vector<std::string> split_by_comma(const std::string& s) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        part = trim(part);
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

// 工具函数：将字符串转换为小写
std::string to_lower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return std::tolower(c);
        });
    return res;
}

// 列元数据（名称+类型）
struct Column {
    std::string name;
    DataType type;

    Column(const std::string& n, DataType t) : name(n), type(t) {}
};

// 行数据（列值的映射）
class Row {
private:
    std::map<std::string, Value> values; // 存储原始列名，匹配时转小写

public:
    // 优化：替换operator[]为insert_or_assign，避免默认构造（C++17+）
    void set_value(const std::string& col_name, const Value& val) {
        values.insert_or_assign(col_name, val);
    }

    Value get_value(const std::string& col_name) const {
        // 大小写不敏感查找列
        std::string col_lower = to_lower(col_name);
        for (const auto& pair : values) {
            if (to_lower(pair.first) == col_lower) {
                return pair.second;
            }
        }
        throw std::runtime_error("Column not found: " + col_name);
    }

    // 获取所有列名
    std::vector<std::string> get_column_names() const {
        std::vector<std::string> cols;
        for (const auto& pair : values) {
            cols.push_back(pair.first);
        }
        return cols;
    }

    // 检查是否包含指定列（大小写不敏感）
    bool has_column(const std::string& col_name) const {
        std::string col_lower = to_lower(col_name);
        for (const auto& pair : values) {
            if (to_lower(pair.first) == col_lower) {
                return true;
            }
        }
        return false;
    }
};

// 表类（管理列结构和行数据）
class Table {
private:
    std::string name;
    std::vector<Column> columns;
    std::vector<Row> rows;

    // 检查列是否存在（大小写不敏感）
    bool has_column(const std::string& col_name) const {
        std::string col_lower = to_lower(col_name);
        return std::any_of(columns.begin(), columns.end(),
            [&](const Column& col) { return to_lower(col.name) == col_lower; });
    }

    // 获取列的类型（大小写不敏感）
    DataType get_column_type(const std::string& col_name) const {
        std::string col_lower = to_lower(col_name);
        auto it = std::find_if(columns.begin(), columns.end(),
            [&](const Column& col) { return to_lower(col.name) == col_lower; });
        if (it == columns.end()) {
            throw std::runtime_error("Column not found: " + col_name);
        }
        return it->type;
    }

public:
    Table(const std::string& n, const std::vector<Column>& cols) : name(n), columns(cols) {}

    // 插入行
    void insert(const Row& row) {
        // 验证列名和类型（强制检查所有列都存在，大小写不敏感）
        for (const auto& col : columns) {
            if (!row.has_column(col.name)) {
                throw std::runtime_error("Missing column: " + col.name);
            }
            Value val = row.get_value(col.name);
            if (val.get_type() != col.type) {
                throw std::runtime_error("Type mismatch for column: " + col.name + " (expected " + (col.type == DataType::INT ? "INT" : "STRING") + ")");
            }
        }
        rows.push_back(row);
    }

    // 查询行（支持WHERE条件：col = val）
    std::vector<Row> select(const std::vector<std::string>& select_cols,
        const std::pair<std::string, Value>* where_clause = nullptr) const {
        std::vector<Row> result;

        for (const auto& row : rows) {
            // 应用WHERE条件
            bool match = true;
            if (where_clause != nullptr) {
                const std::string& col = where_clause->first;
                const Value& val = where_clause->second;
                if (!has_column(col)) {
                    throw std::runtime_error("Column not found in WHERE: " + col);
                }
                if (row.get_value(col) != val) {
                    match = false;
                }
            }

            if (match) {
                // 构建结果行（只包含选中的列）
                Row new_row;
                if (select_cols.empty() || select_cols[0] == "*") {
                    // 选择所有列
                    for (const auto& col : columns) {
                        new_row.set_value(col.name, row.get_value(col.name));
                    }
                }
                else {
                    // 选择指定列
                    for (const std::string& col : select_cols) {
                        if (!has_column(col)) {
                            throw std::runtime_error("Column not found in SELECT: " + col);
                        }
                        // 找到原始列名（用于保持输出一致）
                        std::string original_col_name;
                        for (const auto& c : columns) {
                            if (to_lower(c.name) == to_lower(col)) {
                                original_col_name = c.name;
                                break;
                            }
                        }
                        new_row.set_value(original_col_name, row.get_value(col));
                    }
                }
                result.push_back(new_row);
            }
        }

        return result;
    }

    // 更新行（支持WHERE条件）
    void update(const std::pair<std::string, Value>& set_clause,
        const std::pair<std::string, Value>* where_clause = nullptr) {
        const std::string& set_col = set_clause.first;
        const Value& set_val = set_clause.second;

        // 验证列和类型
        if (!has_column(set_col)) {
            throw std::runtime_error("Column not found in SET: " + set_col);
        }
        if (set_val.get_type() != get_column_type(set_col)) {
            throw std::runtime_error("Type mismatch for column: " + set_col);
        }

        // 执行更新
        for (auto& row : rows) {
            bool match = true;
            if (where_clause != nullptr) {
                const std::string& col = where_clause->first;
                const Value& val = where_clause->second;
                if (!has_column(col)) {
                    throw std::runtime_error("Column not found in WHERE: " + col);
                }
                if (row.get_value(col) != val) {
                    match = false;
                }
            }
            if (match) {
                // 找到原始列名
                std::string original_col_name;
                for (const auto& c : columns) {
                    if (to_lower(c.name) == to_lower(set_col)) {
                        original_col_name = c.name;
                        break;
                    }
                }
                row.set_value(original_col_name, set_val);
            }
        }
    }

    // 删除行（支持WHERE条件）
    void delete_rows(const std::pair<std::string, Value>* where_clause = nullptr) {
        if (where_clause == nullptr) {
            // 删除所有行
            rows.clear();
            return;
        }

        // 验证列存在
        const std::string& col = where_clause->first;
        if (!has_column(col)) {
            throw std::runtime_error("Column not found in WHERE: " + col);
        }

        // 删除匹配的行
        const Value& val = where_clause->second;
        rows.erase(std::remove_if(rows.begin(), rows.end(),
            [&](const Row& row) { return row.get_value(col) == val; }),
            rows.end());
    }

    // 获取列信息
    const std::vector<Column>& get_columns() const { return columns; }

    // 获取表名
    const std::string& get_name() const { return name; }
};

// 数据库核心类（管理多个表，处理SQL语句）
class Database {
private:
    std::map<std::string, Table> tables; // key为小写表名

    // 分割字符串（按空格，忽略引号内的空格）
    std::vector<std::string> split_sql(const std::string& sql) {
        std::vector<std::string> tokens;
        std::string token;
        bool in_quote = false;

        for (char c : sql) {
            if (c == '\'') {
                in_quote = !in_quote;
                token += c;
            }
            else if (c == ' ' && !in_quote) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            }
            else {
                token += c;
            }
        }

        if (!token.empty()) {
            tokens.push_back(token);
        }

        return tokens;
    }

    // 转换字符串到数据类型
    DataType str_to_type(const std::string& type_str) {
        std::string upper_type = to_lower(type_str); // 改为转小写判断
        if (upper_type == "int") return DataType::INT;
        if (upper_type == "string") return DataType::STRING;
        throw std::runtime_error("Unknown data type: " + type_str);
    }

    // 转换字符串到Value（根据类型）
    Value str_to_value(const std::string& val_str, DataType type) {
        if (type == DataType::INT) {
            try {
                // 去除可能的多余字符（如逗号、括号）
                std::string num_str;
                for (char c : val_str) {
                    if (std::isdigit(c) || c == '-') {
                        num_str += c;
                    }
                }
                return Value(std::stoi(num_str));
            }
            catch (...) {
                throw std::runtime_error("Invalid integer value: " + val_str);
            }
        }
        else {
            // 去除引号和多余字符
            std::string str = val_str;
            if (!str.empty() && str.front() == '\'') {
                str = str.substr(1);
            }
            if (!str.empty() && str.back() == '\'') {
                str = str.substr(0, str.size() - 1);
            }
            return Value(str);
        }
    }

    // 从SQL语句中提取表名（适用于INSERT/SELECT/UPDATE/DELETE）
    std::string get_table_name(const std::vector<std::string>& tokens, size_t pos) {
        if (pos >= tokens.size()) {
            throw std::runtime_error("Invalid SQL syntax: missing table name");
        }
        return tokens[pos];
    }

public:
    // 执行SQL语句
    void execute(const std::string& sql) {
        // 不再将整个SQL转大写，保留原始大小写
        auto tokens = split_sql(sql);
        std::string original_sql = sql;

        if (tokens.empty()) return;

        // 处理CREATE TABLE（关键字大小写不敏感）
        if (to_lower(tokens[0]) == "create" && to_lower(tokens[1]) == "table") {
            if (tokens.size() < 3) {
                throw std::runtime_error("Invalid CREATE TABLE syntax: missing table name");
            }

            std::string table_name = tokens[2];
            std::string table_name_lower = to_lower(table_name);
            if (tables.find(table_name_lower) != tables.end()) {
                throw std::runtime_error("Table already exists: " + table_name);
            }

            // 提取括号内的列定义
            std::string col_defs_str = extract_bracketed_content(original_sql);
            std::vector<std::string> col_defs = split_by_comma(col_defs_str);
            std::vector<Column> columns;

            for (const auto& def : col_defs) {
                std::stringstream ss(def);
                std::string col_name, type_str;
                ss >> col_name >> type_str;
                if (col_name.empty() || type_str.empty()) {
                    throw std::runtime_error("Invalid column definition: " + def);
                }
                columns.emplace_back(col_name, str_to_type(type_str));
            }

            tables.emplace(table_name_lower, Table(table_name, columns));
            std::cout << "Table " << table_name << " created successfully." << std::endl;
        }

        // 处理INSERT INTO（关键字大小写不敏感）
        else if (to_lower(tokens[0]) == "insert" && to_lower(tokens[1]) == "into") {
            std::string table_name = get_table_name(tokens, 2);
            std::string table_name_lower = to_lower(table_name);
            auto table_it = tables.find(table_name_lower);
            if (table_it == tables.end()) {
                throw std::runtime_error("Table not found: " + table_name);
            }

            Table& table = table_it->second;
            Row row;

            // 提取列列表和值列表
            size_t values_pos = original_sql.find("VALUES");
            // 兼容小写values
            if (values_pos == std::string::npos) {
                values_pos = original_sql.find("values");
            }
            if (values_pos == std::string::npos) {
                throw std::runtime_error("Invalid INSERT syntax: missing VALUES clause");
            }

            std::string cols_part = original_sql.substr(0, values_pos);
            std::string vals_part = original_sql.substr(values_pos);

            std::string cols_str = extract_bracketed_content(cols_part);
            std::string vals_str = extract_bracketed_content(vals_part);

            std::vector<std::string> cols = split_by_comma(cols_str);
            std::vector<std::string> vals = split_by_comma(vals_str);

            if (cols.size() != vals.size()) {
                throw std::runtime_error("Column and value count mismatch (columns: " + std::to_string(cols.size()) + ", values: " + std::to_string(vals.size()) + ")");
            }

            // 设置行数据
            const auto& columns = table.get_columns();
            for (size_t i = 0; i < cols.size(); ++i) {
                const std::string& col_name = cols[i];
                const std::string& val_str = vals[i];

                // 查找列类型
                auto col_it = std::find_if(columns.begin(), columns.end(),
                    [&](const Column& col) { return to_lower(col.name) == to_lower(col_name); });
                if (col_it == columns.end()) {
                    throw std::runtime_error("Column not found: " + col_name);
                }
                DataType type = col_it->type;

                Value val = str_to_value(val_str, type);
                row.set_value(col_name, val);
            }

            table.insert(row);
            std::cout << "1 row inserted into " << table_name << "." << std::endl;
        }

        // 处理SELECT（关键字大小写不敏感）
        else if (to_lower(tokens[0]) == "select") {
            // 解析选中的列
            std::vector<std::string> select_cols;
            std::string cols_str = tokens[1];
            if (cols_str == "*") {
                select_cols.push_back("*");
            }
            else {
                select_cols = split_by_comma(cols_str);
            }

            // 解析表名（FROM后面的token，关键字大小写不敏感）
            size_t from_pos = std::string::npos;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (to_lower(tokens[i]) == "from") {
                    from_pos = i + 1;
                    break;
                }
            }
            if (from_pos == std::string::npos || from_pos >= tokens.size()) {
                throw std::runtime_error("Invalid SELECT syntax: missing FROM clause or table name");
            }
            std::string table_name = tokens[from_pos];
            std::string table_name_lower = to_lower(table_name);
            auto table_it = tables.find(table_name_lower);
            if (table_it == tables.end()) {
                throw std::runtime_error("Table not found: " + table_name);
            }
            Table& table = table_it->second;

            // 解析WHERE条件（可选，关键字大小写不敏感）
            std::pair<std::string, Value>* where_clause = nullptr;
            std::pair<std::string, Value> where_pair;

            size_t where_pos = std::string::npos;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (to_lower(tokens[i]) == "where") {
                    where_pos = i;
                    break;
                }
            }

            if (where_pos != std::string::npos && where_pos + 3 <= tokens.size()) {
                std::string col_name = tokens[where_pos + 1];
                std::string op = tokens[where_pos + 2];
                std::string val_str = tokens[where_pos + 3];

                if (op != "=") {
                    throw std::runtime_error("Only '=' is supported in WHERE clause");
                }

                // 查找列类型
                auto col_it = std::find_if(table.get_columns().begin(), table.get_columns().end(),
                    [&](const Column& col) { return to_lower(col.name) == to_lower(col_name); });
                if (col_it == table_it->second.get_columns().end()) {
                    throw std::runtime_error("Column not found: " + col_name);
                }
                DataType type = col_it->type;

                Value val = str_to_value(val_str, type);
                where_pair = { col_name, val };
                where_clause = &where_pair;
            }

            // 执行查询
            auto result = table.select(select_cols, where_clause);

            // 输出结果
            std::cout << "Query result (" << result.size() << " rows):" << std::endl;
            // 打印表头
            const auto& columns = table.get_columns();
            std::vector<std::string> display_cols = select_cols[0] == "*" ?
                [&]() {
                std::vector<std::string> cols;
                for (const auto& col : columns) cols.push_back(col.name);
                return cols;
                }() : select_cols;

            for (const auto& col : display_cols) {
                std::cout << std::setw(15) << col;
            }
            std::cout << std::endl;

            // 打印行数据
            for (const auto& row : result) {
                for (const auto& col : display_cols) {
                    Value val = row.get_value(col);
                    std::cout << std::setw(15) << val.to_string();
                }
                std::cout << std::endl;
            }
        }

        // 处理UPDATE（关键字大小写不敏感）
        else if (to_lower(tokens[0]) == "update") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Invalid UPDATE syntax: missing table name");
            }
            std::string table_name = tokens[1];
            std::string table_name_lower = to_lower(table_name);
            auto table_it = tables.find(table_name_lower);
            if (table_it == tables.end()) {
                throw std::runtime_error("Table not found: " + table_name);
            }
            Table& table = table_it->second;

            // 解析SET子句（col = val，关键字大小写不敏感）
            std::pair<std::string, Value> set_clause;
            size_t set_pos = std::string::npos;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (to_lower(tokens[i]) == "set") {
                    set_pos = i;
                    break;
                }
            }
            if (set_pos == std::string::npos || set_pos + 3 > tokens.size()) {
                throw std::runtime_error("Invalid UPDATE syntax: missing or invalid SET clause");
            }

            std::string col_name = tokens[set_pos + 1];
            std::string op = tokens[set_pos + 2];
            std::string val_str = tokens[set_pos + 3];

            if (op != "=") {
                throw std::runtime_error("Only '=' is supported in SET clause");
            }

            // 查找列类型
            auto col_it = std::find_if(table.get_columns().begin(), table.get_columns().end(),
                [&](const Column& col) { return to_lower(col.name) == to_lower(col_name); });
            if (col_it == table.get_columns().end()) {
                throw std::runtime_error("Column not found: " + col_name);
            }
            DataType type = col_it->type;

            Value val = str_to_value(val_str, type);
            set_clause = { col_name, val };

            // 解析WHERE条件（可选，关键字大小写不敏感）
            std::pair<std::string, Value>* where_clause = nullptr;
            std::pair<std::string, Value> where_pair;

            size_t where_pos = std::string::npos;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (to_lower(tokens[i]) == "where") {
                    where_pos = i;
                    break;
                }
            }

            if (where_pos != std::string::npos && where_pos + 3 <= tokens.size()) {
                std::string where_col = tokens[where_pos + 1];
                std::string where_op = tokens[where_pos + 2];
                std::string where_val_str = tokens[where_pos + 3];

                if (where_op != "=") {
                    throw std::runtime_error("Only '=' is supported in WHERE clause");
                }

                // 查找列类型
                auto col_it = std::find_if(table.get_columns().begin(), table.get_columns().end(),
                    [&](const Column& col) { return to_lower(col.name) == to_lower(where_col); });
                if (col_it == table.get_columns().end()) {
                    throw std::runtime_error("Column not found: " + where_col);
                }
                DataType where_type = col_it->type;

                Value where_val = str_to_value(where_val_str, where_type);
                where_pair = { where_col, where_val };
                where_clause = &where_pair;
            }

            // 执行更新
            table.update(set_clause, where_clause);
            std::cout << "Rows updated in " << table_name << "." << std::endl;
        }

        // 处理DELETE（关键字大小写不敏感）
        else if (to_lower(tokens[0]) == "delete" && to_lower(tokens[1]) == "from") {
            if (tokens.size() < 3) {
                throw std::runtime_error("Invalid DELETE syntax: missing table name");
            }
            std::string table_name = tokens[2];
            std::string table_name_lower = to_lower(table_name);
            auto table_it = tables.find(table_name_lower);
            if (table_it == tables.end()) {
                throw std::runtime_error("Table not found: " + table_name);
            }
            Table& table = table_it->second;

            // 解析WHERE条件（可选，关键字大小写不敏感）
            std::pair<std::string, Value>* where_clause = nullptr;
            std::pair<std::string, Value> where_pair;

            size_t where_pos = std::string::npos;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (to_lower(tokens[i]) == "where") {
                    where_pos = i;
                    break;
                }
            }

            if (where_pos != std::string::npos && where_pos + 3 <= tokens.size()) {
                std::string col_name = tokens[where_pos + 1];
                std::string op = tokens[where_pos + 2];
                std::string val_str = tokens[where_pos + 3];

                if (op != "=") {
                    throw std::runtime_error("Only '=' is supported in WHERE clause");
                }

                // 查找列类型
                auto col_it = std::find_if(table.get_columns().begin(), table.get_columns().end(),
                    [&](const Column& col) { return to_lower(col.name) == to_lower(col_name); });
                if (col_it == table.get_columns().end()) {
                    throw std::runtime_error("Column not found: " + col_name);
                }
                DataType type = col_it->type;

                Value val = str_to_value(val_str, type);
                where_pair = { col_name, val };
                where_clause = &where_pair;
            }

            // 执行删除
            table.delete_rows(where_clause);
            std::cout << "Rows deleted from " << table_name << "." << std::endl;
        }

        else {
            throw std::runtime_error("Unsupported SQL command: " + tokens[0]);
        }
    }
};

// 主函数（测试数据库功能）
int main() {
    Database db;

    try {
        // 测试CREATE TABLE
        db.execute("CREATE TABLE student (id INT, name STRING, age INT)");

        // 测试INSERT INTO
        db.execute("INSERT INTO student (id, name, age) VALUES (1, 'Alice', 20)");
        db.execute("INSERT INTO student (id, name, age) VALUES (2, 'Bob', 21)");
        db.execute("INSERT INTO student (id, name, age) VALUES (3, 'Charlie', 22)");

        // 测试SELECT *
        std::cout << "\n--- SELECT * FROM student ---" << std::endl;
        db.execute("SELECT * FROM student");

        // 测试SELECT 指定列 + WHERE
        std::cout << "\n--- SELECT name, age FROM student WHERE id = 2 ---" << std::endl;
        db.execute("SELECT name,age FROM student WHERE id = 2");

        // 测试UPDATE
        std::cout << "\n--- UPDATE student SET age = 23 WHERE name = 'Bob' ---" << std::endl;
        db.execute("UPDATE student SET age = 23 WHERE name = 'Bob'");
        db.execute("SELECT * FROM student WHERE name = 'Bob'");

        // 测试DELETE
        std::cout << "\n--- DELETE FROM student WHERE id = 3 ---" << std::endl;
        db.execute("DELETE FROM student WHERE id = 3");
        db.execute("SELECT * FROM student");

        // 测试DELETE ALL
        std::cout << "\n--- DELETE FROM student ---" << std::endl;
        db.execute("DELETE FROM student");
        db.execute("SELECT * FROM student");
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
