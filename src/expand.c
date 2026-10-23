#include "expand.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int g_last_status = 0;

static char *expand_string(const char *str) {
    size_t cap = 128;
    char *res = xmalloc(cap);
    size_t len = 0;
    
    int sq = 0;
    int dq = 0;
    const char *p = str;
    
    while (*p) {
        if (!sq && !dq) {
            if (*p == '\'') { sq = 1; p++; continue; }
            if (*p == '"') { dq = 1; p++; continue; }
            if (*p == '\\') { 
                p++; 
                if (*p) {
                    if (len + 1 >= cap) { cap *= 2; res = xrealloc(res, cap); }
                    res[len++] = *p++;
                }
                continue; 
            }
        } else if (sq) {
            if (*p == '\'') { sq = 0; p++; continue; }
        } else if (dq) {
            if (*p == '"') { dq = 0; p++; continue; }
            if (*p == '\\') {
                if (p[1] == '"' || p[1] == '\\' || p[1] == '$') {
                    p++;
                }
            }
        }

        if (!sq && *p == '$') {
            p++;
            if (*p == '?') {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", g_last_status);
                size_t blen = strlen(buf);
                if (len + blen >= cap) { cap = (len + blen) * 2; res = xrealloc(res, cap); }
                memcpy(res + len, buf, blen);
                len += blen;
                p++;
                continue;
            } else if (*p == '{') {
                p++;
                const char *start = p;
                while (*p && *p != '}') p++;
                if (*p == '}') {
                    char *vname = xmalloc(p - start + 1);
                    memcpy(vname, start, p - start);
                    vname[p - start] = '\0';
                    char *val = getenv(vname);
                    free(vname);
                    if (val) {
                        size_t vlen = strlen(val);
                        if (len + vlen >= cap) { cap = (len + vlen) * 2; res = xrealloc(res, cap); }
                        memcpy(res + len, val, vlen);
                        len += vlen;
                    }
                    p++;
                } else {
                    if (len + 2 >= cap) { cap *= 2; res = xrealloc(res, cap); }
                    res[len++] = '$';
                    res[len++] = '{';
                }
                continue;
            } else {
                const char *start = p;
                while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_') p++;
                if (p > start) {
                    char *vname = xmalloc(p - start + 1);
                    memcpy(vname, start, p - start);
                    vname[p - start] = '\0';
                    char *val = getenv(vname);
                    free(vname);
                    if (val) {
                        size_t vlen = strlen(val);
                        if (len + vlen >= cap) { cap = (len + vlen) * 2; res = xrealloc(res, cap); }
                        memcpy(res + len, val, vlen);
                        len += vlen;
                    }
                } else {
                    if (len + 1 >= cap) { cap *= 2; res = xrealloc(res, cap); }
                    res[len++] = '$';
                }
                continue;
            }
        }

        if (len + 1 >= cap) { cap *= 2; res = xrealloc(res, cap); }
        res[len++] = *p++;
    }
    
    res[len] = '\0';
    return res;
}

int expand_tokens(TokenList *t) {
    if (!t) return 0;
    for (size_t i = 0; i < t->len; i++) {
        if (t->items[i].type == TOK_WORD) {
            char *exp = expand_string(t->items[i].text);
            free(t->items[i].text);
            t->items[i].text = exp;
        }
    }
    return 0;
}
