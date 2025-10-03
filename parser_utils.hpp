// parser components

int eat_whitespace(const char *s, const char *m, int pos) {
    int ret = 0;
    while(m[pos]==0 && (s[pos]==' ' || s[pos]=='\t' || s[pos]=='\r' || s[pos]=='\n')) {
        ++ret;
        ++pos;
    }
    return ret;
}
int match_string(const char *s, const char *m, int pos, const char *ref) {
    int refl = strlen(ref);
    if(!strncmp(s+pos, ref, refl)) {
        for(int i=pos; i<(pos+refl); ++i) {
            if(m[i]) return 0;
        }
        return refl;
    } else {
        return 0;
    }
}
int match_string_after_ws(const char *s, const char *m, int pos, const char *ref) {
    int d;
    d = eat_whitespace(s,m,pos);
    pos += d;
    return d+match_string(s,m,pos,ref);
}

int match_eof(const char *s, const char *m, int pos) {
    return !s[pos];
}
int match_while(const char *s, const char *m, int pos, std::function<bool (const char *, const char*, int pos)> test) {
    int d=0;
    while(s[pos+d] && test(s,m,pos+d)) {
        ++d;
    }
    return d;
}
std::string capture_while(const char *s, const char *m, int *pos, std::function<bool (const char *, const char*, int pos)> test) {
    int d=match_while(s,m,*pos,test);
    std::string ret = std::string(s+*pos, d);
    (*pos)+=d;
    return ret;
}
bool is_alphanum(char c) {
    return (c>='a' && c<='z')||(c>='A' && c<='Z')||(c>='0'&&c<='9');
}
bool is_alphanum_(char c) {
    return (c>='a' && c<='z')||(c>='A' && c<='Z')||(c>='0'&&c<='9')||(c=='_');
}
std::function<bool (const char *, const char*, int pos)> is_mask_eq(char c) {
    return [c] (const char *s, const char *m, int pos) { return m[pos]==c; };
}

std::string capture_ident(const char *s, const char *m, int *pos) {
    return capture_while(s,m,pos,[] (const char *s, const char *m, int pos) { return is_alphanum_(s[pos]); });
}
int try_capture_quoted(const char *s, const char *m, int pos0, std::string *res) {
    int pos = pos0;
    if(!match_string(s,m,pos,"\"")) return 0;
    pos++;
    std::string ret = capture_while(s,m,&pos,[] (const char *s, const char *m, int pos) {
                                        char c=s[pos];
                                        return !(c=='\"' || c==0 || c=='\n'); });
    if(!match_string(s,m,pos,"\"")) return 0;
    pos++;
    *res = ret;
    return pos-pos0;
}

