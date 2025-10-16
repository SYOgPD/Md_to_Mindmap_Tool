#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <direct.h>

#define MAX_LINE_LENGTH 512
#define MAX_TITLE_LENGTH 256
#define MAX_HEADINGS 1000
#define MAX_LEVEL 6
#define MAX_FILENAME 512
#define MAX_PATH 1024

// 标题节点结构
typedef struct HeadingNode {
    int level;
    char text[MAX_TITLE_LENGTH];
    int line_number;
    struct HeadingNode* parent;
    struct HeadingNode* first_child;
    struct HeadingNode* next_sibling;
} HeadingNode;

// 日志条目结构
typedef struct LogEntry {
    char timestamp[64];
    char filename[MAX_PATH];
    char operation[128];
    struct LogEntry* next;
} LogEntry;

// 全局变量
HeadingNode* headings[MAX_HEADINGS];
int heading_count = 0;
HeadingNode* root = NULL;
LogEntry* log_head = NULL;
int total_operations = 0;

// 函数声明
void trim_whitespace(char* str);
bool is_atx_heading(const char* line, int* level, char* title);
bool is_setext_heading(const char* current_line, const char* next_line, int* level, char* title);
HeadingNode* create_node(int level, const char* text, int line_num);
void add_to_tree(HeadingNode* node, int max_level);
void print_tree(HeadingNode* node, int depth, bool is_last, const char* prefix, FILE* output);
void free_tree(HeadingNode* node);
const char* get_icon(int level);
void parse_markdown_file(FILE* file, int max_level);
void print_mind_map(int max_level, FILE* output);
void get_user_input(char* filename, int* max_level);
void generate_output_filename(const char* input_filename, char* output_filename);
void clear_input_buffer();
void extract_path_and_name(const char* full_path, char* path, char* name);
void add_log_entry(const char* filename, const char* operation);
void show_log_history();
void clear_screen();
void show_main_menu();
void process_single_file();
void free_logs();

// 清屏函数
void clear_screen() {
    #ifdef _WIN32
    system("cls");
    #else
    system("clear");
    #endif
}

// 去除字符串首尾空白字符
void trim_whitespace(char* str) {
    if (str == NULL || strlen(str) == 0) return;
    
    char* start = str;
    char* end = str + strlen(str) - 1;
    
    while (isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

// 清除输入缓冲区
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 添加日志条目
void add_log_entry(const char* filename, const char* operation) {
    LogEntry* new_entry = (LogEntry*)malloc(sizeof(LogEntry));
    if (new_entry == NULL) {
        return;
    }
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(new_entry->timestamp, sizeof(new_entry->timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    strncpy(new_entry->filename, filename, MAX_PATH - 1);
    new_entry->filename[MAX_PATH - 1] = '\0';
    
    strncpy(new_entry->operation, operation, 127);
    new_entry->operation[127] = '\0';
    
    new_entry->next = log_head;
    log_head = new_entry;
    total_operations++;
}

// 显示日志历史
void show_log_history() {
    clear_screen();
    printf("==========================================\n");
    printf("               操作日志历史\n");
    printf("==========================================\n\n");
    
    if (log_head == NULL) {
        printf("暂无操作记录\n\n");
        return;
    }
    
    printf("总操作次数: %d\n\n", total_operations);
    
    LogEntry* current = log_head;
    int count = 1;
    
    while (current != NULL) {
        printf("%d. [%s]\n", count++, current->timestamp);
        printf("   文件: %s\n", current->filename);
        printf("   操作: %s\n\n", current->operation);
        current = current->next;
    }
    
    printf("按任意键返回主菜单...");
    getchar();
}

// 释放日志内存
void free_logs() {
    LogEntry* current = log_head;
    while (current != NULL) {
        LogEntry* next = current->next;
        free(current);
        current = next;
    }
    log_head = NULL;
}

// 从完整路径中提取目录路径和文件名
void extract_path_and_name(const char* full_path, char* path, char* name) {
    char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
    
    #ifdef _WIN32
    _splitpath_s(full_path, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);
    #else
    // 对于非Windows系统，使用简单的方法
    const char* last_slash = strrchr(full_path, '/');
    if (last_slash) {
        strncpy(path, full_path, last_slash - full_path + 1);
        path[last_slash - full_path + 1] = '\0';
        strcpy(name, last_slash + 1);
    } else {
        strcpy(path, "./");
        strcpy(name, full_path);
    }
    return;
    #endif
    
    // 构建路径
    strcpy(path, drive);
    strcat(path, dir);
    
    // 构建文件名
    strcpy(name, fname);
    strcat(name, ext);
}

// 生成输出文件名
void generate_output_filename(const char* input_filename, char* output_filename) {
    char path[MAX_PATH], name[MAX_FILENAME];
    extract_path_and_name(input_filename, path, name);
    
    // 去除原扩展名
    char* dot = strrchr(name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    
    // 构建输出文件路径
    strcpy(output_filename, path);
    strcat(output_filename, name);
    strcat(output_filename, "_mindmap.txt");
}

// 检查是否为ATX格式标题 - 严格匹配以#开头且回车符结尾的格式
bool is_atx_heading(const char* line, int* level, char* title) {
    // 空行检查
    if (!line || *line == '\0') {
        return false;
    }
    
    const char* ptr = line;
    int count = 0;
    
    // 计算行首的#数量
    while (*ptr == '#') {
        count++;
        ptr++;
    }
    
    // 严格匹配：至少1个#，最多6个#，#后必须有空格分隔
    if (count > 0 && count <= MAX_LEVEL && *ptr == ' ') {
        *level = count;
        
        // 跳过空格
        while (*ptr == ' ') ptr++;
        
        // 提取标题文本，直到行尾或换行符
        int i = 0;
        while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r' && i < MAX_TITLE_LENGTH - 1) {
            title[i++] = *ptr++;
        }
        title[i] = '\0';
        
        // 检查行尾是否为换行符或回车符（表示完整的行）
        if (*ptr == '\n' || *ptr == '\r') {
            return true;
        }
    }
    
    return false;
}

// 检查是否为Setext格式标题
bool is_setext_heading(const char* current_line, const char* next_line, int* level, char* title) {
    if (strlen(current_line) == 0 || current_line[0] == '#') {
        return false;
    }
    
    const char* ptr = next_line;
    bool all_equals = true;
    bool all_dashes = true;
    
    while (*ptr != '\0' && *ptr != '\n') {
        if (*ptr != '=') all_equals = false;
        if (*ptr != '-') all_dashes = false;
        ptr++;
    }
    
    if (all_equals || all_dashes) {
        *level = all_equals ? 1 : 2;
        strcpy(title, current_line);
        trim_whitespace(title);
        return true;
    }
    
    return false;
}

// 创建新节点
HeadingNode* create_node(int level, const char* text, int line_num) {
    HeadingNode* node = (HeadingNode*)malloc(sizeof(HeadingNode));
    if (node == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    node->level = level;
    strncpy(node->text, text, MAX_TITLE_LENGTH - 1);
    node->text[MAX_TITLE_LENGTH - 1] = '\0';
    node->line_number = line_num;
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;
    
    return node;
}

// 将节点添加到树中
void add_to_tree(HeadingNode* node, int max_level) {
    if (node->level > max_level) {
        free(node);
        return;
    }
    
    if (heading_count == 0) {
        node->parent = root;
        root->first_child = node;
        headings[heading_count++] = node;
        return;
    }
    
    HeadingNode* parent = headings[heading_count - 1];
    
    while (parent != root && parent->level >= node->level) {
        parent = parent->parent;
    }
    
    node->parent = parent;
    
    if (parent->first_child == NULL) {
        parent->first_child = node;
    } else {
        HeadingNode* sibling = parent->first_child;
        while (sibling->next_sibling != NULL) {
            sibling = sibling->next_sibling;
        }
        sibling->next_sibling = node;
    }
    
    if (heading_count < MAX_HEADINGS) {
        headings[heading_count++] = node;
    }
}

// 打印树结构
void print_tree(HeadingNode* node, int depth, bool is_last, const char* prefix, FILE* output) {
    if (node == NULL) return;
    
    char current_prefix[256] = "";
    if (depth > 0) {
        strcpy(current_prefix, prefix);
        strcat(current_prefix, is_last ? "└── " : "├── ");
    }
    
    const char* icon = get_icon(node->level);
    
    if (node->parent == root) {
        fprintf(output, "%s%s %s\n", current_prefix, icon, node->text);
    } else {
        fprintf(output, "%s%s %s\n", current_prefix, icon, node->text);
    }
    
    char new_prefix[256];
    strcpy(new_prefix, prefix);
    strcat(new_prefix, is_last ? "    " : "│   ");
    
    HeadingNode* child = node->first_child;
    while (child != NULL) {
        HeadingNode* next_child = child->next_sibling;
        bool last_child = (next_child == NULL);
        print_tree(child, depth + 1, last_child, new_prefix, output);
        child = next_child;
    }
}

// 获取级别对应的图标
const char* get_icon(int level) {
    switch (level) {
        case 1: return "[B]";  // Book
        case 2: return "[C]";  // Chapter  
        case 3: return "[S]";  // Section
        case 4: return "[P]";  // Point
        case 5: return "[I]";  // Item
        case 6: return "[L]";  // Label
        default: return "[*]";
    }
}

// 释放树内存
void free_tree(HeadingNode* node) {
    if (node == NULL) return;
    
    HeadingNode* child = node->first_child;
    while (child != NULL) {
        HeadingNode* next_child = child->next_sibling;
        free_tree(child);
        child = next_child;
    }
    
    if (node != root) {
        free(node);
    }
}

// 解析Markdown文件 - 只提取ATX格式标题
void parse_markdown_file(FILE* file, int max_level) {
    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    
    heading_count = 0;
    for (int i = 0; i < MAX_HEADINGS; i++) {
        headings[i] = NULL;
    }
    
    root = create_node(0, "Document Structure", 0);
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        // 不调用trim_whitespace，保持原始行格式以验证回车符结尾
        
        int level;
        char title[MAX_TITLE_LENGTH];
        
        // 只处理ATX格式标题，忽略Setext格式
        if (is_atx_heading(line, &level, title)) {
            HeadingNode* node = create_node(level, title, line_number);
            add_to_tree(node, max_level);
        }
    }
}

// 打印思维导图
void print_mind_map(int max_level, FILE* output) {
    if (root == NULL || root->first_child == NULL) {
        fprintf(output, "No headings found at level %d or below\n", max_level);
        return;
    }
    
    fprintf(output, "[D] Document Structure\n");
    
    HeadingNode* child = root->first_child;
    while (child != NULL) {
        HeadingNode* next_child = child->next_sibling;
        bool last_child = (next_child == NULL);
        print_tree(child, 0, last_child, "", output);
        child = next_child;
    }
}

// 获取用户输入
void get_user_input(char* filename, int* max_level) {
    printf("==========================================\n");
    printf("      Markdown Mind Map Generator\n");
    printf("==========================================\n\n");
    
    while (1) {
        printf("Enter Markdown filename (e.g., document.md or full path): ");
        if (fgets(filename, MAX_FILENAME, stdin) != NULL) {
            trim_whitespace(filename);
            if (strlen(filename) > 0) {
                break;
            }
        }
        printf("Error: Filename cannot be empty, please re-enter!\n");
    }
    
    char level_input[10];
    while (1) {
        printf("Enter maximum heading level (1-%d, default 6): ", MAX_LEVEL);
        if (fgets(level_input, sizeof(level_input), stdin) != NULL) {
            trim_whitespace(level_input);
            if (strlen(level_input) == 0) {
                *max_level = MAX_LEVEL;
                break;
            }
            
            *max_level = atoi(level_input);
            if (*max_level >= 1 && *max_level <= MAX_LEVEL) {
                break;
            }
        }
        printf("Error: Level must be between 1-%d, please re-enter!\n", MAX_LEVEL);
    }
    
    printf("\n");
}

// 显示主菜单
void show_main_menu() {
    clear_screen();
    printf("==========================================\n");
    printf("      Markdown Mind Map Generator\n");
    printf("==========================================\n");
    printf("         Total Operations: %d\n", total_operations);
    printf("==========================================\n\n");
    
    printf("1. Process Markdown File\n");
    printf("2. View Operation History\n");
    printf("3. Clear Screen\n");
    printf("4. Exit\n\n");
    
    printf("Please select an option (1-4): ");
}

// 处理单个文件
void process_single_file() {
    char filename[MAX_FILENAME];
    char output_filename[MAX_FILENAME];
    int max_level;
    
    clear_screen();
    get_user_input(filename, &max_level);
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", filename);
        printf("Please check if the file exists or the path is correct.\n");
        add_log_entry(filename, "Failed to open file");
        printf("Press any key to continue...");
        getchar();
        return;
    }
    
    generate_output_filename(filename, output_filename);
    
    FILE* output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        printf("Error: Cannot create output file %s\n", output_filename);
        fclose(file);
        add_log_entry(filename, "Failed to create output file");
        printf("Press any key to continue...");
        getchar();
        return;
    }
    
    printf("Processing file: %s\n", filename);
    printf("Extracting headings at level %d or below...\n", max_level);
    printf("==========================================\n\n");
    
    parse_markdown_file(file, max_level);
    
    printf("Mind Map Preview:\n");
    printf("------------------------------------------\n");
    print_mind_map(max_level, stdout);
    printf("------------------------------------------\n\n");
    
    // 保存结果到文件
    fprintf(output_file, "Markdown File: %s\n", filename);
    fprintf(output_file, "Extraction Level: Level %d and below\n", max_level);
    fprintf(output_file, "Generated: %s", __DATE__);
    fprintf(output_file, " %s\n", __TIME__);
    fprintf(output_file, "==========================================\n");
    print_mind_map(max_level, output_file);
    fprintf(output_file, "==========================================\n");
    
    printf("Mind map saved to: %s\n\n", output_filename);
    
    // 记录成功操作
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), "Successfully processed, output: %s", output_filename);
    add_log_entry(filename, success_msg);
    
    // 清理资源
    fclose(file);
    fclose(output_file);
    free_tree(root);
    root = NULL;
    
    printf("Press any key to continue...");
    getchar();
    getchar(); // 获取回车键
}

// 主函数
int main() {
    // 设置控制台输出编码（Windows）
    #ifdef _WIN32
    system("chcp 65001 >nul");
    #endif
    
    char choice[10];
    
    while (1) {
        show_main_menu();
        
        if (fgets(choice, sizeof(choice), stdin) != NULL) {
            trim_whitespace(choice);
            
            if (strcmp(choice, "1") == 0) {
                process_single_file();
            }
            else if (strcmp(choice, "2") == 0) {
                show_log_history();
            }
            else if (strcmp(choice, "3") == 0) {
                clear_screen();
            }
            else if (strcmp(choice, "4") == 0) {
                printf("\nThank you for using Markdown Mind Map Generator!\n");
                printf("Total operations performed: %d\n", total_operations);
                break;
            }
            else {
                printf("Invalid option! Please select 1-4.\n");
                printf("Press any key to continue...");
                getchar();
            }
        }
    }
    
    // 程序结束前释放所有内存
    free_logs();
    if (root != NULL) {
        free_tree(root);
    }
    
    return 0;
}