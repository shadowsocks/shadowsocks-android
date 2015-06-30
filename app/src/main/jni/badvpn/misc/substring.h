#include <stddef.h>

#include <misc/debug.h>

static void build_substring_backtrack_table (const char *str, size_t len, size_t *out_table)
{
    ASSERT(len > 0)
    
    size_t x = 0;
    
    for (size_t i = 1; i < len; i++) {
        out_table[i] = x;
        while (x > 0 && str[i] != str[x]) {
            x = out_table[x];
        }
        if (str[i] == str[x]) {
            x++;
        }
    }
}

static int find_substring (const char *text, size_t text_len, const char *word, size_t word_len, const size_t *table, size_t *out_position)
{
    ASSERT(word_len > 0)
    
    size_t x = 0;
    
    for (size_t i = 0; i < text_len; i++) {
        while (x > 0 && text[i] != word[x]) {
            x = table[x];
        }
        if (text[i] == word[x]) {
            if (x + 1 == word_len) {
                *out_position = i - x;
                return 1;
            }
            x++;
        }
    }
    
    return 0;
}

static void build_substring_backtrack_table_reverse (const char *str, size_t len, size_t *out_table)
{
    ASSERT(len > 0)
    
    size_t x = 0;
    
    for (size_t i = 1; i < len; i++) {
        out_table[i] = x;
        while (x > 0 && str[len - 1 - i] != str[len - 1 - x]) {
            x = out_table[x];
        }
        if (str[len - 1 - i] == str[len - 1 - x]) {
            x++;
        }
    }
}

static int find_substring_reverse (const char *text, size_t text_len, const char *word, size_t word_len, const size_t *table, size_t *out_position)
{
    ASSERT(word_len > 0)
    
    size_t x = 0;
    
    for (size_t i = 0; i < text_len; i++) {
        while (x > 0 && text[text_len - 1 - i] != word[word_len - 1 - x]) {
            x = table[x];
        }
        if (text[text_len - 1 - i] == word[word_len - 1 - x]) {
            if (x + 1 == word_len) {
                *out_position = (text_len - 1 - i);
                return 1;
            }
            x++;
        }
    }
    
    return 0;
}
