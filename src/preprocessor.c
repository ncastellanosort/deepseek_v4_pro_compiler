#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_MACROS 256

typedef struct {
    char name[64];
    char *replacement;
    int is_func_like;
    char params[16][64];
    int param_count;
} Macro;

static Macro macros[MAX_MACROS];
static int macro_count;

static int expanding[256];
static int expanding_depth;

/* ── helpers ─────────────────────────────────────────────────────── */

static int find_macro(const char *name) {
    for (int i = 0; i < macro_count; i++)
        if (strcmp(macros[i].name, name) == 0) return i;
    return -1;
}

static int is_expanding(int idx) {
    for (int i = 0; i < expanding_depth; i++)
        if (expanding[i] == idx) return 1;
    return 0;
}

static void extract_dir(const char *filepath, char *dir) {
    const char *slash = strrchr(filepath, '/');
    if (slash) {
        size_t len = slash - filepath + 1;
        memcpy(dir, filepath, len);
        dir[len] = '\0';
    } else {
        dir[0] = '.'; dir[1] = '/'; dir[2] = '\0';
    }
}

/* ── backslash continuation ──────────────────────────────────────── */

static int read_full_line(FILE *in, char *out_buf, int max_len) {
    out_buf[0] = '\0';
    int total = 0;
    char buf[4096];

    while (fgets(buf, sizeof(buf), in)) {
        int len = strlen(buf);
        /* strip trailing newline */
        int end = len;
        while (end > 0 && (buf[end-1] == '\n' || buf[end-1] == '\r')) end--;

        int has_cont = (end > 0 && buf[end-1] == '\\');
        if (has_cont) end--;

        if (total + end >= max_len - 1) break;
        memcpy(out_buf + total, buf, end);
        total += end;
        out_buf[total] = '\0';

        if (!has_cont) return 1;
    }
    return total > 0;
}

/* ── macro argument parsing ──────────────────────────────────────── */

static int parse_macro_args(const char *s, int *pos, char args[16][256]) {
    int i = *pos;
    int arg_starts[16], arg_lens[16];

    while (s[i] && s[i] != '(') i++;
    if (s[i] != '(') return -1;
    i++; /* skip '(' */

    int depth = 1, arg_idx = 0;
    arg_starts[0] = i;
    while (s[i] && depth > 0) {
        if (s[i] == '"') { i++; while (s[i] && s[i] != '"') { if (s[i]=='\\') i++; i++; } if (s[i]) i++; continue; }
        if (s[i] == '\'') { i++; if (s[i]=='\\') i++; if (s[i]) i++; if (s[i]) i++; continue; }
        if (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        else if (s[i] == ',' && depth == 1) {
            arg_lens[arg_idx] = i - arg_starts[arg_idx];
            arg_idx++;
            if (arg_idx >= 16) return -1;
            arg_starts[arg_idx] = i + 1;
        }
        i++;
    }
    if (depth != 0) return -1;
    arg_lens[arg_idx] = i - 1 - arg_starts[arg_idx];
    arg_idx++;

    for (int a = 0; a < arg_idx; a++) {
        int st = arg_starts[a], len = arg_lens[a];
        while (len > 0 && s[st] == ' ') { st++; len--; }
        while (len > 0 && s[st + len - 1] == ' ') len--;
        memcpy(args[a], s + st, len);
        args[a][len] = '\0';
    }

    *pos = i;
    return arg_idx;
}

/* ── forward declarations ────────────────────────────────────────── */

static void process_file(FILE *in, FILE *out, const char *current_dir);
static void expand_line(const char *line, FILE *out);

/* ── expansion ───────────────────────────────────────────────────── */

static void expand_func_macro(int m_idx, const char *arg_start, FILE *out) {
    Macro *m = &macros[m_idx];
    char args[16][256];
    int pos = 0;
    int nargs = parse_macro_args(arg_start, &pos, args);
    if (nargs < 0 || nargs != m->param_count) {
        fprintf(out, "%s", m->name);
        return;
    }

    const char *rep = m->replacement;
    int i = 0;
    while (rep[i]) {
        if (isalpha(rep[i]) || rep[i] == '_') {
            int j = i;
            char ident[64]; int ki = 0;
            while ((isalnum(rep[j]) || rep[j] == '_') && ki < 63)
                ident[ki++] = rep[j++];
            ident[ki] = '\0';

            int param_idx = -1;
            for (int p = 0; p < m->param_count; p++) {
                if (strcmp(ident, m->params[p]) == 0) { param_idx = p; break; }
            }

            if (param_idx >= 0) {
                fprintf(out, "%s", args[param_idx]);
            } else {
                int other = find_macro(ident);
                if (other >= 0 && !is_expanding(other))
                    fprintf(out, "%s", macros[other].replacement);
                else
                    fprintf(out, "%s", ident);
            }
            i = j;
        } else {
            fputc(rep[i], out);
            i++;
        }
    }
}

static void write_macro_expansion(int m_idx, FILE *out) {
    Macro *m = &macros[m_idx];
    if (is_expanding(m_idx)) {
        fprintf(out, "%s", m->name);
        return;
    }
    expanding[expanding_depth++] = m_idx;
    const char *r = m->replacement;

    /* Write the replacement into a temp buffer, then re-expand it */
    /* This handles chained macros like #define B A where A is also a macro */
    char tmp[4096]; int ti = 0;
    int i = 0;
    while (r[i] && ti < (int)sizeof(tmp) - 1) {
        if (isalpha(r[i]) || r[i] == '_') {
            int j = i;
            char ident[64]; int ki = 0;
            while ((isalnum(r[j]) || r[j] == '_') && ki < 63)
                ident[ki++] = r[j++];
            ident[ki] = '\0';

            int sub = find_macro(ident);
            if (sub >= 0 && !is_expanding(sub)) {
                /* re-expand — write to temp buffer then expand_line on it */
                FILE *tf = tmpfile();
                if (tf) {
                    write_macro_expansion(sub, tf);
                    long tsz = ftell(tf);
                    fseek(tf, 0, SEEK_SET);
                    if (tsz > 0 && ti + tsz < (int)sizeof(tmp) - 1) {
                        ti += fread(tmp + ti, 1, tsz, tf);
                    } else {
                        /* fallback: write macro name */
                        memcpy(tmp + ti, ident, ki); ti += ki;
                    }
                    fclose(tf);
                } else {
                    memcpy(tmp + ti, ident, ki); ti += ki;
                }
                i = j;
            } else {
                if (ti + ki < (int)sizeof(tmp) - 1) {
                    memcpy(tmp + ti, ident, ki); ti += ki;
                }
                i = j;
            }
        } else {
            tmp[ti++] = r[i++];
        }
    }
    tmp[ti] = '\0';
    expanding_depth--;

    /* Now scan the expanded text for more macros */
    expand_line(tmp, out);
}

static void expand_line(const char *line, FILE *out) {
    int i = 0;
    while (line[i]) {
        if (line[i] == '"') {
            fputc(line[i], out); i++;
            while (line[i] && line[i] != '"') {
                if (line[i] == '\\') { fputc(line[i], out); i++; }
                fputc(line[i], out); i++;
            }
            if (line[i]) { fputc(line[i], out); i++; }
            continue;
        }
        if (line[i] == '\'') {
            fputc(line[i], out); i++;
            if (line[i] == '\\') { fputc(line[i], out); i++; }
            if (line[i]) { fputc(line[i], out); i++; }
            if (line[i] == '\'') { fputc(line[i], out); i++; }
            continue;
        }
        if (isalpha(line[i]) || line[i] == '_') {
            int j = i;
            char ident[64]; int ki = 0;
            while ((isalnum(line[j]) || line[j] == '_') && ki < 63)
                ident[ki++] = line[j++];
            ident[ki] = '\0';

            int m_idx = find_macro(ident);
            if (m_idx >= 0 && macros[m_idx].is_func_like && line[j] == '(') {
                expand_func_macro(m_idx, line + j, out);
                /* skip past the call */
                while (line[j] && line[j] != '(') j++;
                if (line[j] == '(') {
                    int depth = 1; j++;
                    while (line[j] && depth > 0) {
                        if (line[j] == '"') { j++; while (line[j] && line[j]!='"') { if(line[j]=='\\')j++; j++; } if(line[j])j++; continue; }
                        if (line[j] == '\'') { j++; if(line[j]=='\\')j++; if(line[j])j++; if(line[j])j++; continue; }
                        if (line[j] == '(') depth++;
                        else if (line[j] == ')') depth--;
                        j++;
                    }
                }
                i = j;
            } else if (m_idx >= 0 && !macros[m_idx].is_func_like) {
                write_macro_expansion(m_idx, out);
                i = j;
            } else {
                fprintf(out, "%.*s", (int)(j - i), line + i);
                i = j;
            }
        } else {
            fputc(line[i], out);
            i++;
        }
    }
}

/* ── directives ──────────────────────────────────────────────────── */

static void handle_define(const char *line) {
    int i = 7; /* skip "#define" */
    while (line[i] == ' ') i++;
    char name[64]; int ni = 0;
    while ((isalnum(line[i]) || line[i] == '_') && ni < 63)
        name[ni++] = line[i++];
    name[ni] = '\0';
    if (ni == 0) return;

    Macro *m = &macros[macro_count++];
    strncpy(m->name, name, 63); m->name[63] = '\0';
    m->param_count = 0;
    m->is_func_like = 0;

    if (line[i] == '(') {
        m->is_func_like = 1;
        i++;
        while (line[i] && line[i] != ')') {
            while (line[i] == ' ') i++;
            char pname[64]; int pi = 0;
            while ((isalnum(line[i]) || line[i] == '_') && pi < 63)
                pname[pi++] = line[i++];
            pname[pi] = '\0';
            if (pi > 0 && m->param_count < 16) {
                strncpy(m->params[m->param_count], pname, 63);
                m->params[m->param_count][63] = '\0';
                m->param_count++;
            }
            while (line[i] == ' ') i++;
            if (line[i] == ',') { i++; continue; }
        }
        if (line[i] == ')') i++;
    }

    while (line[i] == ' ') i++;

    size_t rlen = strlen(line + i);
    while (rlen > 0 && (line[i + rlen - 1] == '\n' || line[i + rlen - 1] == '\r' || line[i + rlen - 1] == ' '))
        rlen--;
    m->replacement = malloc(rlen + 1);
    memcpy(m->replacement, line + i, rlen);
    m->replacement[rlen] = '\0';
}

/* ── main processing ─────────────────────────────────────────────── */

static void process_file(FILE *in, FILE *out, const char *current_dir) {
    char full[8192];

    while (read_full_line(in, full, sizeof(full))) {
        char *line = full;
        while (*line == ' ' || *line == '\t') line++;

        if (strncmp(line, "#include", 8) == 0) {
            int i = 8;
            while (line[i] == ' ') i++;
            if (line[i] != '"') {
                fprintf(stderr, "Preprocessor: #include solo soporta comillas dobles\n");
                continue;
            }
            i++;
            char fname[256]; int fi = 0;
            while (line[i] && line[i] != '"' && fi < 255)
                fname[fi++] = line[i++];
            fname[fi] = '\0';

            char fullpath[512];
            if (fname[0] == '/')
                snprintf(fullpath, sizeof(fullpath), "%s", fname);
            else
                snprintf(fullpath, sizeof(fullpath), "%s%s", current_dir, fname);

            FILE *inc = fopen(fullpath, "r");
            if (!inc) {
                fprintf(stderr, "Preprocessor: no se pudo abrir '%s'\n", fullpath);
                continue;
            }

            char inc_dir[512];
            extract_dir(fullpath, inc_dir);

            process_file(inc, out, inc_dir);
            fclose(inc);
        } else if (strncmp(line, "#define", 7) == 0) {
            handle_define(line);
            fputc('\n', out);
        } else {
            expand_line(line, out);
            fputc('\n', out);
        }
    }
}

/* ── public API ──────────────────────────────────────────────────── */

char *preprocess(const char *input_file) {
    FILE *in = fopen(input_file, "r");
    if (!in) {
        fprintf(stderr, "Preprocessor: no se pudo abrir '%s'\n", input_file);
        return NULL;
    }

    char dir[512];
    extract_dir(input_file, dir);

    const char *tmpname = "build/preprocessed.pp";
    char *out_path = malloc(strlen(tmpname) + 1);
    strcpy(out_path, tmpname);
    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Preprocessor: no se pudo crear archivo temporal\n");
        fclose(in);
        free(out_path);
        return NULL;
    }

    macro_count = 0;
    expanding_depth = 0;

    process_file(in, out, dir);

    for (int i = 0; i < macro_count; i++)
        free(macros[i].replacement);

    fclose(in);
    fclose(out);
    return out_path;
}
