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
    M_ACTIVE
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
 
#define FAIL(s,...) { fprintf(stderr, "ERROR: " s "\n" __VA_OPT__(,) __VA_ARGS__); return false; }
struct ConfyFile {
    std::string fname;
    int size;
    char *data;
    char *mask;
    SyntaxNode *s;

    struct {
        std::string line, block_start, block_end, meta_line, meta_block_start, meta_block_end;
    } setup;

    bool parseSetup() {
        int pos=0, d;
        while(pos<size) {
            if(d=match_string(data, mask, pos, "confy-setup")) {
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

                    printf("key: %s val: '%s'\n", key.c_str(), value.c_str());

                    pos+=eat_whitespace(data, mask, pos);
                    pos+=match_string(data, mask, pos, ","); 
                }
                if(match_string(data, mask, pos, "}")) return true;
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
                }
                break;
            case Mask::M_INERT:
                if(is_line_comment) {
                    // currently inside inactivated line of source code, transition back after newline
                    if(d=match_string(data, mask, pos, "\n")) {
                        paintMask(pos0, d+pos-pos0, st);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    }
                } else {
                    // currently inside inactivated block, transition back on comment close
                    if(d=match_string(data, mask, pos, setup.block_end.c_str())) {
                        paintMask(pos0, pos-pos0, st);
                        paintMask(pos, d, Mask::M_INERT_BLOCK_OUT);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    }
                }
                break;
            case Mask::M_META:
                if(is_line_comment) {
                    // currently inside line of confy metainstructions, transition back after newline
                    if(d=match_string(data, mask, pos, "\n")) {
                        paintMask(pos0, d+pos-pos0, st);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    }
                } else {
                    // currently inside inactivated block, transition back on comment close
                    if(d=match_string(data, mask, pos, setup.meta_block_end.c_str())) {
                        paintMask(pos0, pos-pos0, st);
                        paintMask(pos, d, Mask::M_META_BLOCK_OUT);
                        pos=pos0=pos+d;
                        st=Mask::M_ACTIVE;
                    }
                }
                break;
            }
            ++pos;
        }
        // paint remainder of block
        paintMask(pos0, pos-pos0, st);
        return true;
    }

    SyntaxNode *parseExpr(int &pos) {
        return NULL;
    }

    #define STR_OR_FAIL(s) \
            if(!(d=match_string(data,mask,pos,s))) return NULL; \
            pos+=d; 

    #define THROW(err...) { char err_buf[1024]; sprintf(err_buf, err); throw std::string(err_buf); }

    #define STR_OR_THROW(s,err...) \
            if(!(d=match_string(data,mask,pos,s))) THROW(err) \
            pos+=d; 


    // '$' <identifier>
    std::string parseVarName(int &pos) {
        int d;
        STR_OR_FAIL("$");
        return capture_ident(data,mask,&pos);
    }

    ConfyVal *parseValue(int &pos) {
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

        ret->name = parseVarName(pos);
        if(!ret->name.length()) return NULL; //TODO throw appropriate error

        pos+=eat_whitespace(data,mask,pos);

        // optional friendly name, like int $winSize "Window Size" = 3;
        pos+=try_capture_quoted(data,mask,pos,&ret->v.display);
        if(!ret->v.display.length()) ret->v.display = ret->name;

        pos+=eat_whitespace(data,mask,pos);

        STR_OR_FAIL("=");
        pos+=eat_whitespace(data,mask,pos);
        
        ret->pre = std::string(data+pos0, pos-pos0);

        ConfyVal *va = parseValue(pos);
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

            STR_OR_FAIL("(");

            cond=parseExpr(pos);

            STR_OR_FAIL(")");

            pos+=eat_whitespace(data,mask,pos);

            STR_OR_FAIL("{");

            pos1=pos;

            body=parseSeq(pos);

            pos2=pos;

            STR_OR_FAIL("}");

            pos+=eat_whitespace(data,mask,pos);

            if(d=match_string(data,mask,pos,"else")) {
                IfThenElse *s = new IfThenElse();
                s->pre = std::string(data+pos0, pos1-pos0);
                pos+=eat_whitespace(data,mask,pos);
                SyntaxNode *alt;
                if(!(alt=parseIf(pos))) {
                    STR_OR_FAIL("{");
                    s->inter = std::string(data+pos2, pos-pos2);
                    alt=parseSeq(pos);
                    STR_OR_FAIL("}");
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
            fprintf(stderr, "PARSE ERROR: %s\n", err.c_str());
            return false;
        }
        return (s!=NULL);
    }
};


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
        printf("== Debug render: ==\n%s", f.s->Render(&f,this).c_str());

        return true;
    }

};

#include "ast_impl.hpp"

int main(int argc, char* argv[])
{
    ConfyState st;
    if(argc>1) st.LoadAndParseFile(argv[1]);
    return 0;
}

