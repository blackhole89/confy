#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <functional>

#include <stdio.h>
#include <malloc.h>
#include <string.h>

enum class Mask : char {
    M_META=0,
    M_META_LINE_IN,
    M_META_BLOCK_IN,
    M_META_BLOCK_OUT,
    M_INERT,
    M_INERT_LINE_IN,
    M_INERT_BLOCK_IN,
    M_INERT_BLOCK_OUT,
    M_ACTIVE,
    M_PROTECTED
};

struct ConfyFile;

enum ConfyType {
    T_BOOL,
    T_INT,
    T_FLOAT,
    T_STRING
};

struct ConfyVal {
    ConfyType t;
    bool b;
    int i;
    double f;
    std::string s;

    std::string Render() {
        char buf[64];
        switch(t) {
        case T_BOOL: return b?"true":"false";
        case T_INT: 
            sprintf(buf,"%d",i);
            return buf;
        case T_FLOAT:
            sprintf(buf,"%f",f);
            return buf;
        case T_STRING:
            return "\""+s+"\"";
        default: return "\"<CORRUPTED>\"";
        }
    }
};

struct ConfyVar {
    ConfyFile *fl;
    std::string display;
    ConfyVal val;
    bool hidden;
};    

#include "parser_utils.hpp"

#include "ast_def.hpp"

ConfyVal boolVal(bool b) {
    return ConfyVal { T_BOOL, b, b?1:0, b?1.0:0.0, b?"true":"false" };
}
ConfyVal intVal(int i) {
    char buf[64];
    sprintf(buf,"%d",i);
    return ConfyVal { T_INT, i!=0, i, (double)i, buf };
}
ConfyVal floatVal(double f) {
    char buf[512];
    sprintf(buf,"%f",f);
    return ConfyVal { T_FLOAT, ((int)f)!=0, (int)f, f, buf };
}


struct OpTier {
    std::vector<const char*> ops;
    std::function<ConfyVal (ConfyVal, ConfyVal, int)> ev_fun;
};

OpTier opTiers[] = {
    { {"||"}, [] (ConfyVal v1,ConfyVal v2,int type) { return boolVal(v1.b || v2.b); } },
    { {"&&"}, [] (ConfyVal v1,ConfyVal v2,int type) { return boolVal(v1.b && v2.b); } },
    { {"==", "!="}, [] (ConfyVal l,ConfyVal r,int type) { 
                                 bool res;
                                 switch(l.t) {
                                 case T_BOOL: res = l.b == r.b; break;
                                 case T_INT: res = l.i == r.i; break;
                                 case T_FLOAT: res = l.f == r.f; break;
                                 case T_STRING: res = l.s == r.s; break;
                                 default: res = false;
                                 }
                                 return boolVal(type ^ res);
                                }
                               },
    { {"<=", "<", ">=", ">"}, [] (ConfyVal v1,ConfyVal v2,int type) { 
                                 if(v1.t == T_FLOAT) {
                                    switch(type) { case 0: return boolVal(v1.f<=v2.f);
                                                   case 1: return boolVal(v1.f<v2.f);
                                                   case 2: return boolVal(v1.f>=v2.f);
                                                   case 3: return boolVal(v1.f>v2.f); }
                                 } else {
                                    switch(type) { case 0: return boolVal(v1.i<=v2.i);
                                                   case 1: return boolVal(v1.i<v2.i);
                                                   case 2: return boolVal(v1.i>=v2.i);
                                                   case 3: return boolVal(v1.i>v2.i); }
                                 }
                                 return boolVal(false);
                                } 
                               },
     { {"+","-"}, [] (ConfyVal v1,ConfyVal v2,int type) {
                                 if(v1.t == T_FLOAT) return floatVal(v1.f+(type?-1.0:1.0)*v2.f);
                                 else return intVal(v1.i+(type?-1:1)*v2.i);
                                }
                               }, 
     { {"*","/","%"}, [] (ConfyVal v1,ConfyVal v2,int type) {
                                 if(v1.t == T_FLOAT && type<2)
                                     return type?floatVal(v1.f/v2.f):floatVal(v1.f*v2.f);
                                 else switch(type) {
                                     case 0: return intVal(v1.i*v2.i);
                                     case 1: return intVal(v1.i/v2.i);
                                     case 2: return intVal(v1.i%v2.i);
                                 }
                                 return intVal(0);
                                }
                               },
      { { } } 
};

ConfyVal *parseValue(const char *data, const char *mask, int &pos) {
    int d; std::string str;
    if(d=match_string(data,mask,pos,"true")) {
        pos+=d;
        return new ConfyVal { T_BOOL, true, 1, 1.0f, "true" };
    } else if(d=match_string(data,mask,pos,"false")) {
        pos+=d;
        return new ConfyVal { T_BOOL, false, 0, 0.0f, "false" };
    } else if(d=try_capture_quoted(data,mask,pos,&str)) {
        pos+=d;
        return new ConfyVal { T_STRING, str!="", str!="", (float)(int)(str!=""), str };
    } else { 
        char *endptr;
        int pos0=pos;
        double v = strtod(data+pos, &endptr);
        if(endptr>(data+pos)) {
            pos+=(endptr-(data+pos));
            return new ConfyVal { T_FLOAT, (bool)(int)v, (int)v, v, std::string(data+pos0, pos-pos0) };
        } else {
            return NULL;
        }
    }
}

#define FAIL(s,...) { fprintf(stderr, "ERROR: " s "\n" __VA_OPT__(,) __VA_ARGS__); return false; }
struct ConfyFile {
    std::string fname;
    int size;
    char *data;
    char *mask;
    int setup_start, setup_end;

    SyntaxNode *s;

    struct {
        std::string line, block_start, block_end, meta_line, meta_block_start, meta_block_end;
    } setup;

    bool parseSetup() {
        int pos=0, d;
        while(pos<size) {
            if(d=match_string(data, mask, pos, "confy-setup")) {
                setup_start = pos;
                pos+=d;

                pos+=eat_whitespace(data, mask, pos);
                if(!(d=match_string(data, mask, pos, "{")))
                    FAIL("Expected '{' after confy-setup");
                pos+=d;
                
                pos+=eat_whitespace(data, mask, pos);
                while(!match_string(data, mask, pos, "}") && !match_eof(data, mask, pos)) {
                    pos+=eat_whitespace(data, mask, pos);
                    
                    std::string key = capture_ident(data,mask,&pos);
                    pos+=eat_whitespace(data, mask, pos);
                    if(!(d=match_string(data, mask, pos, ":")))
                        FAIL("Expected ':' after confy-setup key");
                    pos+=d;
                    pos+=eat_whitespace(data, mask, pos);
                    
                    std::string value;
                    if(!(d=try_capture_quoted(data,mask,pos,&value)))
                        FAIL("Expected quoted string for confy-setup value");
                    pos+=d;

                    #define SETUP_STRING(s) if(key == #s) { setup.s = value; } else
                    SETUP_STRING(line)
                    SETUP_STRING(block_start)
                    SETUP_STRING(block_end)
                    SETUP_STRING(meta_line)
                    SETUP_STRING(meta_block_start)
                    SETUP_STRING(meta_block_end)
                    { FAIL("Unknown confy-setup key: '%s'", key.c_str()); }
                    #undef SETUP_STRING

//                    printf("key: %s val: '%s'\n", key.c_str(), value.c_str());

                    pos+=eat_whitespace(data, mask, pos);
                    pos+=match_string(data, mask, pos, ","); 
                }
                if(match_string(data, mask, pos, "}")) {
                    setup_end = pos+1;
                    return true;
                } else return false;
            } else ++pos;
        }
        return false;
    }

    void paintMask(int pos, int d, Mask clr) {
        memset(mask+pos, (char)clr, d);
    }

    bool colourBlocks() {
        int pos=0, pos0=0, d;
        bool is_line_comment;
        Mask st = Mask::M_ACTIVE;

        // overpaint confy-setup block
//        printf("protect %d..%d\n", setup_start, setup_end);
        paintMask(setup_start, setup_end-setup_start, Mask::M_PROTECTED);

        while(pos<size) {
            switch(st) {
            case Mask::M_ACTIVE:
                // currently inside active source code, transition to line or block comments
                if(d=match_string(data, mask, pos, setup.line.c_str())) {
                    paintMask(pos0, pos-pos0, st);
                    paintMask(pos, d, Mask::M_INERT_LINE_IN);
                    pos=pos0=pos+d;
                    st=Mask::M_INERT; is_line_comment=true;
                } else if(d=match_string(data, mask, pos, setup.block_start.c_str())) {
                    paintMask(pos0, pos-pos0, st);
                    paintMask(pos, d, Mask::M_INERT_BLOCK_IN);
                    pos=pos0=pos+d;
                    st=Mask::M_INERT; is_line_comment=false;
                } else if(d=match_string(data, mask, pos, setup.meta_line.c_str())) {
                    paintMask(pos0, pos-pos0, st);
                    paintMask(pos, d, Mask::M_META_LINE_IN);
                    pos=pos0=pos+d;
                    st=Mask::M_META; is_line_comment=true;
                } else if(d=match_string(data, mask, pos, setup.meta_block_start.c_str())) {
                    paintMask(pos0, pos-pos0, st);
                    paintMask(pos, d, Mask::M_META_BLOCK_IN);
                    pos=pos0=pos+d;
                    st=Mask::M_META; is_line_comment=false;
                } else ++pos;
                break;
            case Mask::M_INERT:
                if(is_line_comment) {
                    // currently inside inactivated line of source code, transition back after newline
                    if(d=match_string(data, mask, pos, "\n")) {
                        paintMask(pos0, d+pos-pos0, st);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    } else ++pos;
                } else {
                    // currently inside inactivated block, transition back on comment close
                    if(d=match_string(data, mask, pos, setup.block_end.c_str())) {
                        paintMask(pos0, pos-pos0, st);
                        paintMask(pos, d, Mask::M_INERT_BLOCK_OUT);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    } else ++pos;
                }
                break;
            case Mask::M_META:
                if(is_line_comment) {
                    // currently inside line of confy metainstructions, transition back after newline
                    if(d=match_string(data, mask, pos, "\n")) {
                        paintMask(pos0, d+pos-pos0, st);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    } else ++pos;
                } else {
                    // currently inside inactivated block, transition back on comment close
                    if(d=match_string(data, mask, pos, setup.meta_block_end.c_str())) {
                        paintMask(pos0, pos-pos0, st);
                        paintMask(pos, d, Mask::M_META_BLOCK_OUT);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    } else ++pos;
                }
                break;
            }
        }
        // paint remainder of block
        paintMask(pos0, pos-pos0, st);

        return true;
    }

    #define STR_OR_FAIL(s) \
            if(!(d=match_string(data,mask,pos,s))) return NULL; \
            pos+=d; 

    #define THROW(err...) { char err_buf[1024]; sprintf(err_buf, err); throw std::string(err_buf); }

    #define STR_OR_THROW(s,err...) \
            if(!(d=match_string(data,mask,pos,s))) THROW(err) \
            pos+=d; 

    Expr *parseExpr(int &pos, int tier);
    Expr *parseExprSingleton(int &pos) {
        int d;
        std::string vn;
        if(d=match_string(data,mask,pos,"!")) {
            pos+=d;
            pos+=eat_whitespace(data,mask,pos);
            Expr *sub = parseExprSingleton(pos);
            ExprNot *e = new ExprNot();
            e->sub = sub;
            return e;
        } else if(d=match_string(data,mask,pos,"-")) {
            pos+=d;
            pos+=eat_whitespace(data,mask,pos);
            Expr *sub = parseExprSingleton(pos);
            ExprNeg *e = new ExprNeg();
            e->sub = sub;
            return e;
        } else if(d=match_string(data,mask,pos,"(")) {
            pos+=d;
            pos+=eat_whitespace(data,mask,pos);
            Expr *sub = parseExpr(pos,0);
            STR_OR_THROW(")", "Expected ')'");
            return sub;
        } else if(d=tryVarName(pos, vn)) {
            pos+=d;
            ExprVar *sub = new ExprVar();
            sub->name = vn;
            return sub;
        } else if(ConfyVal *v=parseValue(data,mask,pos)) {
            ExprLiteral *sub = new ExprLiteral();
            sub->v = *v;
            return sub;
        } else THROW("Expected '!', '-', parenthesized expression, variable name or literal");
    }


    SyntaxNode *parseExpr(int &pos) {
        int pos0=pos;

        pos += eat_whitespace(data,mask,pos);
        Expr *sub = parseExpr(pos,0);
        ExprNode *n = new ExprNode();
        n->source = std::string(data+pos0, pos-pos0);
        n->root = sub;

        return n;
    }

    // '$' <identifier>
    int tryVarName(int pos, std::string &n) {
        int pos0=pos;
        int d;
        if(!(d=match_string(data,mask,pos,"$"))) return 0;
        pos+=d;
        n=capture_ident(data,mask,&pos);
        if(!n.length()) THROW("Expected nonempty variable name after '$'");
        return pos-pos0;
    }



    // <type> <varname> ["<friendly name>"] '=' <value> ';'
    SyntaxNode *parseVarDef(int &pos) {
        int pos0=pos, d;
        VarDef *ret;
        if(d=match_string(data,mask,pos,"bool")) {
            ret = new VarDef();
            ret->v.val.t = T_BOOL;
        } else if(d=match_string(data,mask,pos,"string")) {
            ret = new VarDef();
            ret->v.val.t = T_STRING;
        } else if(d=match_string(data,mask,pos,"int")) {
            ret = new VarDef();
            ret->v.val.t = T_INT;
        } else if(d=match_string(data,mask,pos,"float")) {
            ret = new VarDef();
            ret->v.val.t = T_FLOAT;
        } else return NULL;
        pos+=d;
        pos+=eat_whitespace(data,mask,pos);

        ret->name = "";
        pos+=tryVarName(pos,ret->name);
        if(!ret->name.length()) THROW("Expected variable name after type name");

        pos+=eat_whitespace(data,mask,pos);

        // optional friendly name, like int $winSize "Window Size" = 3;
        pos+=try_capture_quoted(data,mask,pos,&ret->v.display);
        if(!ret->v.display.length()) ret->v.display = ret->name;

        pos+=eat_whitespace(data,mask,pos);

        STR_OR_FAIL("=");
        pos+=eat_whitespace(data,mask,pos);
        
        ret->pre = std::string(data+pos0, pos-pos0);

        ConfyVal *va = parseValue(data,mask,pos);
        if(!va) return NULL;

        pos0=pos;

        ret->v.val.b = va->b; ret->v.val.f = va->f; ret->v.val.i = va->i; ret->v.val.s = va->s;
        delete va;

        STR_OR_FAIL(";");

        ret->post = std::string(data+pos0, pos-pos0);

        return ret;
    }

    // 'if' '(' <expression> ')' '{' <sequence> '}' [ 'else'  ( <if> | ( '{' <sequence> '}' ) ) ] 
    SyntaxNode *parseIf(int &pos) {
        int pos0=pos, pos1, pos2, d;
        if(d=match_string(data,mask,pos,"if")) {
            SyntaxNode *cond, *body;

            pos+=d;

            pos+=eat_whitespace(data,mask,pos);

            STR_OR_THROW("(", "'if' must be followed by '('");

            cond=parseExpr(pos);

            STR_OR_THROW(")", "'if' condition must be closed by ')'");

            pos+=eat_whitespace(data,mask,pos);

            STR_OR_FAIL("{");

            pos1=pos;

            body=parseSeq(pos);

            pos2=pos;

            STR_OR_FAIL("}");

            pos+=eat_whitespace(data,mask,pos);

            if(d=match_string(data,mask,pos,"else")) {
                pos+=d;
                IfThenElse *s = new IfThenElse();
                s->pre = std::string(data+pos0, pos1-pos0);
                s->cond = cond;
                s->sub1 = body;
                pos+=eat_whitespace(data,mask,pos);
                SyntaxNode *alt;
                if(!(alt=parseIf(pos))) {
                    STR_OR_THROW("{", "Expected '{' or 'if' after 'else'");
                    s->inter = std::string(data+pos2, pos-pos2);
                    alt=parseSeq(pos);
                    STR_OR_THROW("}", "Expected '}' after else-block");
                    s->post = "}";
                } else {
                    s->post = "";
                }
                s->sub2 = alt;
                return s;
            } else {
                IfThen *s = new IfThen();
                s->pre = std::string(data+pos0, pos1-pos0);
                s->post = std::string(data+pos2, pos-pos2);
                s->cond = cond;
                s->sub = body;
                return s;
            }
        } else return NULL;
    }

    // sequence of basic blocks
    Seq *parseSeq(int &pos) {
        Seq *ret = new Seq;
        int d;
        while(pos<size) {
            SyntaxNode *n;
            if(d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_ACTIVE))) {
                SourceBlock *s = new SourceBlock();
                s->bType = SourceBlock::B_ACTIVE;
                s->contents = std::string(data+pos, d);
                ret->children.push_back(s);
                pos+=d;
            } else if(d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_INERT_LINE_IN))) {
                pos+=d;
                SourceBlock *s = new SourceBlock();
                s->bType = SourceBlock::B_INERT_LINE;
                s->contents = capture_while(data,mask,&pos,is_mask_eq((char)Mask::M_INERT));
                ret->children.push_back(s);
            } else if(d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_INERT_BLOCK_IN))) {
                pos+=d;
                SourceBlock *s = new SourceBlock();
                s->bType = SourceBlock::B_INERT_BLOCK;
                s->contents = capture_while(data,mask,&pos,is_mask_eq((char)Mask::M_INERT));
                ret->children.push_back(s);
                d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_INERT_BLOCK_OUT));
                pos+=d;
            } else if(   (d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_META_LINE_IN)))
                      || (d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_META_BLOCK_IN)))
                      || (d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_META_BLOCK_OUT)))
                      || (d=match_while(data,mask,pos,is_mask_eq((char)Mask::M_PROTECTED)))
                      || (d=eat_whitespace(data,mask,pos))
                     ) {
                SourceBlock *s = new SourceBlock();
                s->bType = SourceBlock::B_META_CHAFF;
                s->contents = std::string(data+pos, d);
                ret->children.push_back(s);
                pos+=d;
            } else if(match_eof(data,mask,pos) || match_string(data, mask, pos, "}")) {
                break;
            } else if(n=parseIf(pos)) {
                ret->children.push_back(n); 
            } else if(n=parseVarDef(pos)) {
                ret->children.push_back(n);
            } else {
                THROW("Unexpected '%s' in mode '%d'", data+pos, mask[pos]);
                return NULL;
            }
        }
        return ret;
    }
    bool parseBody() {
        int pos=0;
        try {
            s=parseSeq(pos);
        } catch(std::string err) {
            fprintf(stderr, "PARSE ERROR at '%s' byte %d: %s\n", fname.c_str(), pos, err.c_str());
            fprintf(stderr, "TAIL: %.64s\n", data+pos);
            return false;
        }
        return (s!=NULL);
    }
};

Expr *ConfyFile::parseExpr(int &pos, int tier) {
    int d;

    // bottom of precedence list
    if(!opTiers[tier].ops.size()) {
        return parseExprSingleton(pos);
    }

    ExprOp *ret = new ExprOp();

    Expr *sub = parseExpr(pos, tier+1),*sub1;

    ret->op = opTiers[tier].ev_fun;
    ret->subexprs.push_back(sub);
    ret->subtypes.push_back(0);

    while(1) {
        pos += eat_whitespace(data,mask,pos);
        int index=0;
        for(;index<opTiers[tier].ops.size();++index) {
            if(d=match_string(data,mask,pos,opTiers[tier].ops[index])) break; 
        }
        if(index==opTiers[tier].ops.size()) break; // can't extend this expr tier
        pos += d;

        pos += eat_whitespace(data,mask,pos);
        sub1 = parseExpr(pos, tier+1);
        if(!sub1) THROW("Expected expression after '%s'", opTiers[tier].ops[index]);
        ret->subexprs.push_back(sub1);
        ret->subtypes.push_back(index);
    }
    if(ret->subexprs.size()==1) {
        delete ret;
        return sub;
    }
    return ret;
}


struct ConfyState {
    std::vector<ConfyFile> files;

    std::map<std::string, ConfyVar> vars; 

    bool LoadAndParseFile(std::string fname) {
        auto size = std::filesystem::file_size(fname);
        if(size<=0) {
            fprintf(stderr,"ERROR: File '%s' not found.\n",fname.c_str());
            return false;
        }

        files.push_back(ConfyFile());
        ConfyFile &f = files.back();

        f.size = size;
        f.fname = fname;

        /* read file contents */
        FILE *fl = fopen(fname.c_str(),"rb");
        if(!fl) {
            fprintf(stderr,"ERROR: Could not open file '%s'.\n",fname.c_str());
            files.pop_back();
            return false;
        }
        f.data = (char*)malloc(size+1);
        if(!fread(f.data,size,1,fl)) {
            fprintf(stderr,"ERROR: Failed to read from '%s'.\n",fname.c_str());
            free(f.data);
            files.pop_back();
            return false;
        }
        f.data[size]=0;
        fclose(fl);
        /* init mask for parsing */
        f.mask = (char*)malloc(size+1);
        memset(f.mask, 0, size+1);

        /* look for confy-setup block */
        if(!f.parseSetup()) {
            fprintf(stderr,"ERROR: Could not find confy-setup block in '%s'.\n",fname.c_str());
            free(f.data);
            free(f.mask);
            files.pop_back();
            return false;
        }
        if(!f.colourBlocks()) {
            fprintf(stderr,"ERROR: Could not segment '%s' according to comment types.\n",fname.c_str());
            free(f.data);
            free(f.mask);
            files.pop_back();
            return false;
        }
        if(!f.parseBody()) {
            fprintf(stderr,"ERROR: Failed to parse '%s'.\n",fname.c_str());
            free(f.data);
            free(f.mask);
            files.pop_back();
            return false;
        }
        f.s->Execute(&f,this,true);
        //printf("== Debug render: ==\n%s", f.s->Render(&f,this).c_str());

        return true;
    }

    bool SaveFile(ConfyFile *f) 
    {
        free(f->data);
        
        std::string data = f->s->Render(f,this);
        f->data = (char*)malloc(data.length()+1);
        memcpy(f->data, data.c_str(), data.length()+1);

        FILE *fl = fopen(f->fname.c_str(),"wb");
        if(!fl) {
            fprintf(stderr,"ERROR: Could not open file '%s' for writing.\n",f->fname.c_str());
            return false;
        }
        if(!fwrite(f->data,data.length(),1,fl)) {
            fprintf(stderr,"ERROR: Failed to write to '%s'.\n",f->fname.c_str());
            return false;
        }
        fclose(fl);
        return true;
    }
};

#include "ast_impl.hpp"

int main(int argc, char* argv[])
{
    ConfyState st;
    if(argc>1) st.LoadAndParseFile(argv[1]);
    if(argc>3) {
        if(!strcmp(argv[2], "get")) {
            if(st.vars.count(argv[3])) {
                printf("%s\n",st.vars[argv[3]].val.Render().c_str());
                return 0;
            } else {
                fprintf(stderr,"Variable '%s' not found\n", argv[3]);
                return -1;
            }
        } else if(!strcmp(argv[2], "set") && argc>4) {
            if(st.vars.count(argv[3])) {
                // prepare mask to parse value from argv[4]
                int len = strlen(argv[3])+1, pos=0;
                char *mask = (char*)malloc(len);
                memset(mask,0,len);
                ConfyVal *newv = parseValue(argv[4], mask, pos);
                if(!newv) {
                    fprintf(stderr,"Couldn't parse value '%s'!\n",argv[4]);
                    return -2;
                }

                // replace value in state
                ConfyType oldt = st.vars[argv[3]].val.t;
                st.vars[argv[3]].val = *newv;
                st.vars[argv[3]].val.t = oldt; // coerce to definitional type

                // reevaluate script and save
                st.vars[argv[3]].fl->s->Execute(st.vars[argv[3]].fl, &st, true);
                st.SaveFile(st.vars[argv[3]].fl);
                printf("== Debug render: ==\n%s", st.vars[argv[3]].fl->s->Render(st.vars[argv[3]].fl, &st).c_str());
                return 0;
            } else {
                fprintf(stderr,"Variable '%s' not found\n", argv[3]);
                return -1;
            }
        }
    }
    return 0;
}

