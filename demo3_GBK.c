#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 512
#define MAX_TITLE_LENGTH 256
#define MAX_HEADINGS 1000
#define MAX_LEVEL 6
#define MAX_FILENAME 256

// 标题节点结构
typedef struct HeadingNode {
    int level;
    char text[MAX_TITLE_LENGTH];
    int line_number;
    struct HeadingNode* parent;
    struct HeadingNode* first_child;
    struct HeadingNode* next_sibling;
} HeadingNode;

// 全局变量
HeadingNode* headings[MAX_HEADINGS];
int heading_count = 0;
HeadingNode* root = NULL;

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

// 去除字符串首尾空白字符
void trim_whitespace(char* str) {
    if (str == NULL || strlen(str) == 0) return;
    
    char* start = str;
    char* end = str + strlen(str) - 1;
    
    // 去除首部空白
    while (isspace((unsigned char)*start)) start++;
    
    // 去除尾部空白
    while (end > start && isspace((unsigned char)*end)) end--;
    
    // 移动字符
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

// 清除输入缓冲区
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 获取用户输入
void get_user_input(char* filename, int* max_level) {
    printf("==========================================\n");
    printf("      Markdown 思维导图生成器\n");
    printf("==========================================\n\n");
    
    // 获取文件名
    while (1) {
        printf("请输入Markdown文件名（例如：document.md）: ");
        if (fgets(filename, MAX_FILENAME, stdin) != NULL) {
            trim_whitespace(filename);
            if (strlen(filename) > 0) {
                break;
            }
        }
        printf("错误：文件名不能为空，请重新输入！\n");
    }
    
    // 获取最大级别
    char level_input[10];
    while (1) {
        printf("请输入要提取的最大标题级别(1-%d，默认6): ", MAX_LEVEL);
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
        printf("错误：级别必须在1-%d之间，请重新输入！\n", MAX_LEVEL);
    }
    
    printf("\n");
}

// 生成输出文件名
void generate_output_filename(const char* input_filename, char* output_filename) {
    strcpy(output_filename, input_filename);
    
    // 查找最后一个点号（文件扩展名）
    char* dot = strrchr(output_filename, '.');
    if (dot != NULL) {
        *dot = '\0'; // 去除原扩展名
    }
    
    // 添加新扩展名
    strcat(output_filename, "_mindmap.txt");
}

// 检查是否为ATX格式标题 (# ## ###)
bool is_atx_heading(const char* line, int* level, char* title) {
    const char* ptr = line;
    int count = 0;
    
    // 统计#的数量
    while (*ptr == '#') {
        count++;
        ptr++;
    }
    
    // 检查是否有空格分隔，且不超过最大级别
    if (count > 0 && count <= MAX_LEVEL && isspace((unsigned char)*ptr)) {
        *level = count;
        
        // 跳过空格
        while (isspace((unsigned char)*ptr)) ptr++;
        
        strcpy(title, ptr);
        trim_whitespace(title);
        
        // 去除尾部可能存在的#
        int len = strlen(title);
        while (len > 0 && title[len-1] == '#') {
            title[--len] = '\0';
        }
        trim_whitespace(title);
        
        return true;
    }
    
    return false;
}

// 检查是否为Setext格式标题 (下划线风格)
bool is_setext_heading(const char* current_line, const char* next_line, int* level, char* title) {
    if (strlen(current_line) == 0 || current_line[0] == '#') {
        return false;
    }
    
    const char* ptr = next_line;
    bool all_equals = true;
    bool all_dashes = true;
    
    // 检查下一行是否全为=或-
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
    
    // 如果是第一个节点，作为根节点的子节点
    if (heading_count == 0) {
        node->parent = root;
        root->first_child = node;
        headings[heading_count++] = node;
        return;
    }
    
    // 寻找父节点
    HeadingNode* parent = headings[heading_count - 1];
    
    // 如果当前节点级别小于等于前一个节点，需要回溯找到合适的父节点
    while (parent != root && parent->level >= node->level) {
        parent = parent->parent;
    }
    
    // 设置父节点关系
    node->parent = parent;
    
    // 添加到父节点的子节点列表
    if (parent->first_child == NULL) {
        parent->first_child = node;
    } else {
        // 找到最后一个兄弟节点
        HeadingNode* sibling = parent->first_child;
        while (sibling->next_sibling != NULL) {
            sibling = sibling->next_sibling;
        }
        sibling->next_sibling = node;
    }
    
    // 添加到 headings 数组
    if (heading_count < MAX_HEADINGS) {
        headings[heading_count++] = node;
    }
}

// 打印树结构
void print_tree(HeadingNode* node, int depth, bool is_last, const char* prefix, FILE* output) {
    if (node == NULL) return;
    
    // 构建当前行的前缀
    char current_prefix[256] = "";
    if (depth > 0) {
        strcpy(current_prefix, prefix);
        strcat(current_prefix, is_last ? "\\-- " : "|-- ");
    }
    
    // 获取图标
    const char* icon = get_icon(node->level);
    
    // 打印当前节点
    if (node->parent == root) {
        // 根节点的直接子节点
        fprintf(output, "%s%s %s\n", current_prefix, icon, node->text);
    } else {
        fprintf(output, "%s%s %s\n", current_prefix, icon, node->text);
    }
    
    // 构建新的前缀
    char new_prefix[256];
    strcpy(new_prefix, prefix);
    strcat(new_prefix, is_last ? "    " : "|   ");
    
    // 递归打印子节点
    HeadingNode* child = node->first_child;
    while (child != NULL) {
        HeadingNode* next_child = child->next_sibling;
        bool last_child = (next_child == NULL);
        print_tree(child, depth + 1, last_child, new_prefix, output);
        child = next_child;
    }
}

// 获取级别对应的图标 - 使用简单字符替代emoji
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

// 解析Markdown文件
void parse_markdown_file(FILE* file, int max_level) {
    char line[MAX_LINE_LENGTH];
    char prev_line[MAX_LINE_LENGTH] = "";
    int line_number = 0;
    
    // 重置全局变量
    heading_count = 0;
    for (int i = 0; i < MAX_HEADINGS; i++) {
        headings[i] = NULL;
    }
    
    // 创建根节点
    root = create_node(0, "Document Structure", 0);
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        trim_whitespace(line);
        
        int level;
        char title[MAX_TITLE_LENGTH];
        
        // 检查ATX标题
        if (is_atx_heading(line, &level, title)) {
            HeadingNode* node = create_node(level, title, line_number);
            add_to_tree(node, max_level);
        }
        // 检查Setext标题（需要前一行的内容）
        else if (line_number > 1 && is_setext_heading(prev_line, line, &level, title)) {
            HeadingNode* node = create_node(level, title, line_number - 1);
            add_to_tree(node, max_level);
        }
        
        // 保存当前行作为下一轮的前一行
        strncpy(prev_line, line, sizeof(prev_line) - 1);
        prev_line[sizeof(prev_line) - 1] = '\0';
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

// 主函数
int main() {
    char filename[MAX_FILENAME];
    char output_filename[MAX_FILENAME];
    int max_level;
    
    // 设置控制台输出编码（Windows）
    #ifdef _WIN32
    system("chcp 65001 >nul");
    #endif
    
    // 获取用户输入
    get_user_input(filename, &max_level);
    
    // 打开Markdown文件
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", filename);
        printf("Please check if the file exists or the path is correct.\n");
        printf("Press any key to exit...");
        getchar();
        return 1;
    }
    
    // 生成输出文件名
    generate_output_filename(filename, output_filename);
    
    // 打开输出文件
    FILE* output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        printf("Error: Cannot create output file %s\n", output_filename);
        fclose(file);
        printf("Press any key to exit...");
        getchar();
        return 1;
    }
    
    printf("Processing file: %s\n", filename);
    printf("Extracting headings at level %d or below...\n", max_level);
    printf("==========================================\n\n");
    
    // 解析文件并生成思维导图
    parse_markdown_file(file, max_level);
    
    // 在控制台显示结果
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
    
    // 清理资源
    fclose(file);
    fclose(output_file);
    free_tree(root);
    
    printf("Press any key to exit...");
    getchar();
    
    return 0;
}