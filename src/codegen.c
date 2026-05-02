#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *out;
static int label_counter, str_counter;

/* loop stack: start, continue_target, end */
#define MAX_LOOP 64
static int ls_start[MAX_LOOP], ls_cont[MAX_LOOP], ls_end[MAX_LOOP];
static int loop_depth;

/* locals */
#define MAX_LOCALS 128
static struct { char name[MAX_LEXEME]; int offset; } locals[MAX_LOCALS];
static int local_count, next_offset, in_function;

/* declared arrays */
#define MAX_ARRAYS 64
static struct { char name[MAX_LEXEME]; int size; } arrays[MAX_ARRAYS];
static int array_count;

/* helpers */
static void locals_clear(void) { local_count=0; next_offset=-8; }
static int local_find(const char *n) { for(int i=0;i<local_count;i++) if(!strcmp(locals[i].name,n)) return locals[i].offset; return 0; }
static int local_add(const char *n) { int o=next_offset; strncpy(locals[local_count].name,n,MAX_LEXEME-1); locals[local_count].name[MAX_LEXEME-1]=0; locals[local_count].offset=o; local_count++; next_offset-=8; return o; }
static int new_label(void) { return label_counter++; }
static int new_str(void) { return str_counter++; }
static void push_loop(int s, int c, int e) { if(loop_depth>=MAX_LOOP){fprintf(stderr,"Error: bucles\n");exit(1);} ls_start[loop_depth]=s; ls_cont[loop_depth]=c; ls_end[loop_depth]=e; loop_depth++; }
static void pop_loop(void) { if(loop_depth>0)loop_depth--; }
static const char *arg_reg(int i) { static const char *r[]={"%rdi","%rsi","%rdx","%rcx","%r8","%r9"}; return i<6?r[i]:NULL; }
static int is_arr(const char *n) { for(int i=0;i<array_count;i++) if(!strcmp(arrays[i].name,n)) return 1; return 0; }

void codegen_init(const char *fn) {
    out=fopen(fn,"w"); if(!out){fprintf(stderr,"Error: '%s'\n",fn);exit(1);}
    label_counter=0; str_counter=0; loop_depth=0; array_count=0;
}

/* fwd */
static void gen_expr(ASTNode *n);
static void gen_stmt_list(ASTNode *first);
static void gen_index_addr(ASTNode *n);

/* ── PRINT ──────────────────────────────────────────────────────── */
static int is_str(ASTNode *n) { return n&&n->type==AST_STRING; }
static void emit_print(const char *fmt) {
    fprintf(out,"\tmovq\t%%rax, %%rsi\n\tleaq\t%s(%%rip), %%rdi\n\txorl\t%%eax, %%eax\n\tcall\tprintf@PLT\n",fmt);
}
static void gen_print(ASTNode *n) {
    gen_expr(n->left); emit_print(is_str(n->left)?"fmtS":"fmtD");
}

/* ── CALL ───────────────────────────────────────────────────────── */
static void gen_call(ASTNode *n) {
    int c=0; for(ASTNode *a=n->left;a;a=a->next){gen_expr(a);fprintf(out,"\tpushq\t%%rax\n");c++;}
    if(c>6){fprintf(stderr,"Error: max 6 args\n");exit(1);}
    for(int i=c-1;i>=0;i--)fprintf(out,"\tpopq\t%s\n",arg_reg(i));
    fprintf(out,"\txorl\t%%eax, %%eax\n\tcall\t%s\n",n->var_name);
}

/* ── RETURN ─────────────────────────────────────────────────────── */
static void gen_return(ASTNode *n) { gen_expr(n->left); fprintf(out,"\tleave\n\tret\n"); }

/* ── ASSIGN ─────────────────────────────────────────────────────── */
static void gen_assign(ASTNode *n) {
    ASTNode *t=n->left, *v=n->right;
    if(t->type==AST_VARIABLE){gen_expr(v);int o=local_find(t->var_name);
        if(o)fprintf(out,"\tmovq\t%%rax, %d(%%rbp)\n",o);
        else if(in_function){o=local_add(t->var_name);fprintf(out,"\tmovq\t%%rax, %d(%%rbp)\n",o);}
        else fprintf(out,"\tmovq\t%%rax, %s(%%rip)\n",t->var_name);}
    else if(t->type==AST_INDEX){gen_index_addr(t);fprintf(out,"\tpushq\t%%rax\n");gen_expr(v);fprintf(out,"\tpopq\t%%rcx\n\tmovq\t%%rax, (%%rcx)\n");}
    else if(t->type==AST_DEREF){gen_expr(t->left);fprintf(out,"\tpushq\t%%rax\n");gen_expr(v);fprintf(out,"\tpopq\t%%rcx\n\tmovq\t%%rax, (%%rcx)\n");}
}

/* ── BINARY ─────────────────────────────────────────────────────── */
static const char *set_insn(char op) { switch(op){case OP_EQ:return"sete";case OP_NE:return"setne";case'<':return"setl";case'>':return"setg";case OP_LE:return"setle";case OP_GE:return"setge";default:return NULL;} }
static void gen_binary(ASTNode *n) {
    gen_expr(n->left);fprintf(out,"\tpushq\t%%rax\n");gen_expr(n->right);
    char op=n->op;
    if(set_insn(op)){fprintf(out,"\tpopq\t%%rcx\n\tcmpq\t%%rax, %%rcx\n\t%s\t%%al\n\tmovzbl\t%%al, %%eax\n",set_insn(op));return;}
    switch(op){
        case'+':fprintf(out,"\tpopq\t%%rcx\n\taddq\t%%rcx, %%rax\n");break;
        case'-':fprintf(out,"\tpopq\t%%rcx\n\tsubq\t%%rax, %%rcx\n\tmovq\t%%rcx, %%rax\n");break;
        case'*':fprintf(out,"\tpopq\t%%rcx\n\timulq\t%%rcx, %%rax\n");break;
        case'/':fprintf(out,"\tpopq\t%%rcx\n\txchgq\t%%rax, %%rcx\n\tcqto\n\tidivq\t%%rcx\n");break;
        case'%':fprintf(out,"\tpopq\t%%rcx\n\txchgq\t%%rax, %%rcx\n\tcqto\n\tidivq\t%%rcx\n\tmovq\t%%rdx, %%rax\n");break;
        case'&':fprintf(out,"\tpopq\t%%rcx\n\tandq\t%%rcx, %%rax\n");break;
        case'|':fprintf(out,"\tpopq\t%%rcx\n\torq\t%%rcx, %%rax\n");break;
        case'^':fprintf(out,"\tpopq\t%%rcx\n\txorq\t%%rcx, %%rax\n");break;
        case OP_LSHIFT:fprintf(out,"\tpopq\t%%rcx\n\txchgq\t%%rax, %%rcx\n\tsalq\t%%cl, %%rax\n");break;
        case OP_RSHIFT:fprintf(out,"\tpopq\t%%rcx\n\txchgq\t%%rax, %%rcx\n\tsarq\t%%cl, %%rax\n");break;
        case OP_LAND:fprintf(out,"\tpopq\t%%rcx\n\ttestq\t%%rcx,%%rcx\n\tsetne\t%%cl\n\ttestq\t%%rax,%%rax\n\tsetne\t%%al\n\tandb\t%%cl,%%al\n\tmovzbl\t%%al,%%eax\n");break;
        case OP_LOR:fprintf(out,"\tpopq\t%%rcx\n\ttestq\t%%rcx,%%rcx\n\tsetne\t%%cl\n\ttestq\t%%rax,%%rax\n\tsetne\t%%al\n\torb\t%%cl,%%al\n\tmovzbl\t%%al,%%eax\n");break;
    }
}

/* ── UNARY ──────────────────────────────────────────────────────── */
static void gen_unary(ASTNode *n) {
    gen_expr(n->left);
    switch(n->op){case'm':fprintf(out,"\tnegq\t%%rax\n");break; case OP_NOT:fprintf(out,"\ttestq\t%%rax,%%rax\n\tsete\t%%al\n\tmovzbl\t%%al,%%eax\n");break; case OP_DEREF:fprintf(out,"\tmovq\t(%%rax),%%rax\n");break; default:break;}
}

/* ── LEAF ───────────────────────────────────────────────────────── */
static void gen_number(ASTNode *n) { fprintf(out,"\tmovq\t$%d, %%rax\n",n->num_value); }
/* ── STRING ─────────────────────────────────────────────────────── */
static void emit_escaped(const char *s) {
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\r': fputs("\\r", out); break;
            case '\\': fputs("\\\\", out); break;
            case '"':  fputs("\\\"", out); break;
            default:   fputc(*s, out); break;
        }
    }
    fputc('"', out);
}
static void gen_string(ASTNode *n) { int l=new_str(); fprintf(out,"\t.section .rodata\n.LS%d:\n\t.string ",l); emit_escaped(n->var_name); fprintf(out,"\n\t.text\n\tleaq\t.LS%d(%%rip), %%rax\n",l); }
static void gen_variable(ASTNode *n) { int o=local_find(n->var_name); if(o)fprintf(out,"\tmovq\t%d(%%rbp), %%rax\n",o); else fprintf(out,"\tmovq\t%s(%%rip), %%rax\n",n->var_name); }

/* ── INDEX ──────────────────────────────────────────────────────── */
static void gen_index_addr(ASTNode *n) {
    int o=local_find(n->var_name);
    gen_expr(n->left);
    if(o){
        fprintf(out,"\tleaq\t%d(%%rbp,%%rax,8), %%rax\n",o);
    } else if(is_arr(n->var_name)){
        fprintf(out,"\tpushq\t%%rax\n\tleaq\t%s(%%rip), %%rax\n\tpopq\t%%rcx\n\tleaq\t(%%rax,%%rcx,8), %%rax\n",n->var_name);
    } else {
        fprintf(out,"\tpushq\t%%rax\n\tmovq\t%s(%%rip), %%rax\n\tpopq\t%%rcx\n\tleaq\t(%%rax,%%rcx,8), %%rax\n",n->var_name);
    }
}
static void gen_index(ASTNode *n) { gen_index_addr(n); fprintf(out,"\tmovq\t(%%rax), %%rax\n"); }
static void gen_deref(ASTNode *n) { gen_expr(n->left); fprintf(out,"\tmovq\t(%%rax), %%rax\n"); }
static void gen_addr(ASTNode *n) {
    ASTNode *t=n->left;
    if(t->type==AST_VARIABLE){int o=local_find(t->var_name); if(o)fprintf(out,"\tleaq\t%d(%%rbp), %%rax\n",o); else fprintf(out,"\tleaq\t%s(%%rip), %%rax\n",t->var_name);}
    else if(t->type==AST_INDEX)gen_index_addr(t);
    else if(t->type==AST_DEREF)gen_expr(t->left);
}

/* ── IF / ELSE ──────────────────────────────────────────────────── */
static void gen_if(ASTNode *n) { int e=new_label(); gen_expr(n->left); fprintf(out,"\tcmpq\t$0,%%rax\n\tje\t.L%d\n",e); gen_stmt_list(n->right->next); fprintf(out,".L%d:\n",e); }
static void gen_if_else(ASTNode *ifn,ASTNode *eln) { int el=new_label(),e=new_label(); gen_expr(ifn->left); fprintf(out,"\tcmpq\t$0,%%rax\n\tje\t.L%d\n",el); gen_stmt_list(ifn->right->next); fprintf(out,"\tjmp\t.L%d\n",e); fprintf(out,".L%d:\n",el); gen_stmt_list(eln->right->next); fprintf(out,".L%d:\n",e); }

/* ── WHILE ──────────────────────────────────────────────────────── */
static void gen_while(ASTNode *n) { int s=new_label(),e=new_label(); push_loop(s,s,e); fprintf(out,".L%d:\n",s); gen_expr(n->left); fprintf(out,"\tcmpq\t$0,%%rax\n\tje\t.L%d\n",e); gen_stmt_list(n->right->next); fprintf(out,"\tjmp\t.L%d\n",s); fprintf(out,".L%d:\n",e); pop_loop(); }

/* ── DOWHILE ────────────────────────────────────────────────────── */
static void gen_dowhile(ASTNode *n) {
    int start=new_label(), cond_lbl=new_label(), end=new_label();
    push_loop(start, cond_lbl, end);
    fprintf(out,".L%d:\n",start);
    gen_stmt_list(n->right->next);
    fprintf(out,".L%d:\n",cond_lbl);
    gen_expr(n->left);
    fprintf(out,"\tcmpq\t$0,%%rax\n\tjne\t.L%d\n",start);
    fprintf(out,".L%d:\n",end);
    pop_loop();
}

/* ── TERNARY ────────────────────────────────────────────────────── */
static void gen_ternary(ASTNode *n) {
    int el=new_label(), end=new_label();
    gen_expr(n->left);
    fprintf(out,"\tcmpq\t$0,%%rax\n\tje\t.L%d\n",el);
    gen_expr(n->right);
    fprintf(out,"\tjmp\t.L%d\n",end);
    fprintf(out,".L%d:\n",el);
    gen_expr(n->next);
    fprintf(out,".L%d:\n",end);
}

/* ── FOR ────────────────────────────────────────────────────────── */
static void gen_for(ASTNode *n) {
    int start=new_label(), step_lbl=new_label(), end=new_label();
    push_loop(start, step_lbl, end);
    if(n->left) gen_expr(n->left);
    fprintf(out,".L%d:\n",start);
    ASTNode *body=n->right, *cond=body->left;
    if(cond){gen_expr(cond);fprintf(out,"\tcmpq\t$0,%%rax\n\tje\t.L%d\n",end);}
    ASTNode *prev=NULL, *step=NULL;
    for(ASTNode *s=body->next;s;s=s->next){if(!s->next){step=s;break;} prev=s;}
    if(prev){ASTNode *sv=prev->next;prev->next=NULL;gen_stmt_list(body->next);prev->next=sv;}
    fprintf(out,".L%d:\n",step_lbl);
    if(step)gen_expr(step);
    fprintf(out,"\tjmp\t.L%d\n",start);
    fprintf(out,".L%d:\n",end);
    pop_loop();
}

/* ── BREAK / CONTINUE ──────────────────────────────────────────── */
static void gen_break(void) { if(!loop_depth){fprintf(stderr,"Error: break\n");exit(1);} fprintf(out,"\tjmp\t.L%d\n",ls_end[loop_depth-1]); }
static void gen_continue(void) { if(!loop_depth){fprintf(stderr,"Error: continue\n");exit(1);} fprintf(out,"\tjmp\t.L%d\n",ls_cont[loop_depth-1]); }

/* ── stmt list ─────────────────────────────────────────────────── */
static void gen_stmt_list(ASTNode *first) {
    while(first){
        if(first->type==AST_IF&&first->next&&first->next->type==AST_ELSE){gen_if_else(first,first->next);first=first->next->next;}
        else if(first->type==AST_IF)   {gen_if(first);first=first->next;}
        else if(first->type==AST_WHILE){gen_while(first);first=first->next;}
        else if(first->type==AST_DOWHILE){gen_dowhile(first);first=first->next;}
        else if(first->type==AST_FOR)  {gen_for(first);first=first->next;}
        else if(first->type==AST_BREAK) {gen_break();first=first->next;}
        else if(first->type==AST_CONTINUE){gen_continue();first=first->next;}
        else if(first->type==AST_ELSE){fprintf(stderr,"Error: else huérfano\n");exit(1);}
        else if(first->type==AST_RETURN){gen_return(first);first=first->next;}
        else if(first->type==AST_ARRAY_DECL){first=first->next;}
        else if(first->type==AST_DECL){gen_expr(first);first=first->next;}
        else {gen_expr(first);first=first->next;}
    }
}

/* ── expr dispatch ─────────────────────────────────────────────── */
static void gen_expr(ASTNode *n) {
    if(!n)return;
    switch(n->type){ case AST_PRINT:gen_print(n);break; case AST_CALL:gen_call(n);break; case AST_ASSIGN:gen_assign(n);break; case AST_BINARY:gen_binary(n);break; case AST_UNARY:if(n->op==OP_ADDR)gen_addr(n);else gen_unary(n);break; case AST_NUMBER:gen_number(n);break; case AST_STRING:gen_string(n);break; case AST_VARIABLE:gen_variable(n);break; case AST_INDEX:gen_index(n);break; case AST_DEREF:gen_deref(n);break; case AST_RETURN:gen_return(n);break; case AST_ARRAY_DECL:break; case AST_TERNARY:gen_ternary(n);break; case AST_DECL:{
        int o=local_find(n->var_name);
        if(!o&&in_function) o=local_add(n->var_name);
        if(n->left){gen_expr(n->left);
            if(o)fprintf(out,"\tmovq\t%%rax, %d(%%rbp)\n",o);
            else if(in_function){o=local_add(n->var_name);fprintf(out,"\tmovq\t%%rax, %d(%%rbp)\n",o);}
            else fprintf(out,"\tmovq\t%%rax, %s(%%rip)\n",n->var_name);}
        else {
            if(o)fprintf(out,"\tmovq\t$0, %d(%%rbp)\n",o);
            else if(in_function){o=local_add(n->var_name);fprintf(out,"\tmovq\t$0, %d(%%rbp)\n",o);}
            else fprintf(out,"\tmovq\t$0, %s(%%rip)\n",n->var_name);
        }}break; default:fprintf(stderr,"Error interno: tipo %d\n",n->type);exit(1); }
}

/* ── scan locals ───────────────────────────────────────────────── */
static void scan_locals(ASTNode *n) {
    if(!n)return;
    if(n->type==AST_ASSIGN&&n->left&&n->left->type==AST_VARIABLE)local_add(n->left->var_name);
    if(n->type==AST_DECL)local_add(n->var_name);
    if(n->type==AST_BLOCK||n->type==AST_PROGRAM){for(ASTNode*s=n->next;s;s=s->next)scan_locals(s);}
    if(n->type==AST_IF){scan_locals(n->left);scan_locals(n->right);if(n->next&&n->next->type==AST_ELSE)scan_locals(n->next->right);}
    if(n->type==AST_WHILE||n->type==AST_FOR||n->type==AST_DOWHILE){scan_locals(n->left);scan_locals(n->right);}
    if(n->type==AST_TERNARY){scan_locals(n->left);scan_locals(n->right);scan_locals(n->next);}
    if(n->type==AST_RETURN)scan_locals(n->left);
    if(n->type==AST_PRINT)scan_locals(n->left);
}

/* ── collect variable/array names for BSS ──────────────────────── */
static void collect_vars(ASTNode *n, char vars[][MAX_LEXEME], int *vc,
                          char arrs[][MAX_LEXEME], int *ac) {
    if(!n)return;
    if(n->type==AST_ASSIGN&&n->left&&n->left->type==AST_VARIABLE){
        int found=0;for(int i=0;i<*vc;i++)if(!strcmp(vars[i],n->left->var_name)){found=1;break;}
        if(!found&&*vc<128){strcpy(vars[*vc],n->left->var_name);(*vc)++;}
    }
    if(n->type==AST_DECL){
        int found=0;for(int i=0;i<*vc;i++)if(!strcmp(vars[i],n->var_name)){found=1;break;}
        if(!found&&*vc<128){strcpy(vars[*vc],n->var_name);(*vc)++;}
    }
    if(n->type==AST_INDEX){
        int found=0;for(int i=0;i<*ac;i++)if(!strcmp(arrs[i],n->var_name)){found=1;break;}
        if(!found&&*ac<128){strcpy(arrs[*ac],n->var_name);(*ac)++;}
    }
    collect_vars(n->left,vars,vc,arrs,ac);
    collect_vars(n->right,vars,vc,arrs,ac);
    collect_vars(n->next,vars,vc,arrs,ac);
    if(n->type==AST_PROGRAM||n->type==AST_BLOCK)
        for(ASTNode*s=n->next;s;s=s->next)collect_vars(s,vars,vc,arrs,ac);
    if(n->type==AST_IF&&n->next&&n->next->type==AST_ELSE)
        collect_vars(n->next,vars,vc,arrs,ac);
    if(n->type==AST_FOR){collect_vars(n->left,vars,vc,arrs,ac);collect_vars(n->right,vars,vc,arrs,ac);}
}

/* ── func gen ──────────────────────────────────────────────────── */
static void gen_func_header(ASTNode *func) {
    in_function=1;locals_clear();int pi=0;
    for(ASTNode*p=func->left;p;p=p->next){local_add(p->var_name);pi++;}
    scan_locals(func->right);
    fprintf(out,"\t.globl\t%s\n%s:\n\tpushq\t%%rbp\n\tmovq\t%%rsp, %%rbp\n",func->var_name,func->var_name);
    if(local_count>0){int t=(local_count*8+15)&~15;fprintf(out,"\tsubq\t$%d, %%rsp\n",t);}
    pi=0;for(ASTNode*p=func->left;p;p=p->next){fprintf(out,"\tmovq\t%s, %d(%%rbp)\n",arg_reg(pi),local_find(p->var_name));pi++;}
}
static void gen_func_footer(void){fprintf(out,"\tmovq\t$0, %%rax\n\tleave\n\tret\n");in_function=0;}

/* ── program ───────────────────────────────────────────────────── */
void codegen_program(ASTNode *program) {
    ASTNode *first=program->next;
    int func_mode=(first&&first->type==AST_FUNC);

    /* collect declared arrays */
    for(ASTNode*s=program->next;s;s=s->next)if(s->type==AST_ARRAY_DECL){arrays[array_count].size=s->num_value;strcpy(arrays[array_count].name,s->var_name);array_count++;}

    fprintf(out,"\t.bss\n");
    if(!func_mode){
        char vars[128][MAX_LEXEME];int vc=0;
        char arrs[128][MAX_LEXEME];int ac=0;
        collect_vars(program, vars, &vc, arrs, &ac);
        for(int i=0;i<vc;i++) fprintf(out,"\t.comm\t%s, 8, 8\n",vars[i]);
        for(int i=0;i<array_count;i++)fprintf(out,"\t.comm\t%s, %d, 8\n",arrays[i].name,arrays[i].size*8);
        for(int i=0;i<ac;i++){
            int skip=0;
            for(int j=0;j<vc;j++)if(!strcmp(arrs[i],vars[j])){skip=1;break;}
            if(skip||is_arr(arrs[i]))continue;
            fprintf(out,"\t.comm\t%s, 1024, 8\n",arrs[i]);
        }
    }

    fprintf(out,"\t.section .rodata\nfmtD:\n\t.string \"%%ld\\n\"\nfmtS:\n\t.string \"%%s\\n\"\n\t.text\n\t.extern\tprintf\n");
    if(func_mode){for(ASTNode*f=program->next;f;f=f->next){if(f->type!=AST_FUNC)continue;gen_func_header(f);gen_stmt_list(f->right->next);gen_func_footer();}}
    else {in_function=0;locals_clear();fprintf(out,"\t.globl\tmain\nmain:\n\tpushq\t%%rbp\n\tmovq\t%%rsp, %%rbp\n");gen_stmt_list(program->next);fprintf(out,"\tmovq\t$0, %%rax\n\tleave\n\tret\n");}
}

void codegen_finish(void) { if(out)fclose(out); out=NULL; }
