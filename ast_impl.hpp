// AST functions implementations

std::string Seq::Render(int fid, ConfyState *st) {
    std::string ret;
    for(auto n : children) 
        ret += n->Render(fid, st);
    return ret;
}

ConfyVal Seq::Execute(int fid, ConfyState *st, bool enable) {
    ConfyVal ret;
    for(auto n : children)
        ret = n->Execute(fid,st,enable);
    return ret;
}

std::string VarDef::Render(int fid, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += v.val.Render();
    ret += post;
    return ret;
}

ConfyVal VarDef::Execute(int fid, ConfyState *st, bool enable) {
    if(!st->vars.count(name)) {
        st->vars[name] = v;
        st->vars[name].fl = fid;
        st->vars[name].hidden = hidden;
        st->varNames.push_back(name);
    } else {
        // backprop from state
        v = st->vars[name];
    }
    st->vars[name].hidden = hidden || !enable;
    return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
}

std::string lineify(std::string input, std::string comment)
{
    int pos=0, pos0=0;
    std::string out;
    while( (pos=input.find('\n', pos0)) != std::string::npos) {
        out += comment;
        out += std::string(input, pos0, pos-pos0+1);
        pos0 = pos+1; 
    }
    if(input[pos0]) {
        out += comment;
        out += std::string(input, pos0);
        out += "\n";
    }
    return out;
}

std::string SourceBlock::Render(int fid, ConfyState *st) {
    std::string ret;
    if(bType == B_INERT_LINE) {
        ret = lineify(contents, st->files[fid].setup.line);
    } else if(bType == B_INERT_BLOCK) {
        ret += st->files[fid].setup.block_start;
        ret += contents;
        ret += st->files[fid].setup.block_end;
    } else ret = contents;
    return ret;
}

ConfyVal SourceBlock::Execute(int fid, ConfyState *st, bool enable) {
    if(bType == B_META_CHAFF) return { T_BOOL, true, 1, 1.0, "true" };

    if(enable) bType=B_ACTIVE;
    else if(st->files[fid].setup.line.length() && contents.find('\n')==(contents.length()-1)) bType=B_INERT_LINE;
    else if(st->files[fid].setup.block_start.length()) bType=B_INERT_BLOCK;
    else bType=B_INERT_LINE;

    return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
}

std::string IfThen::Render(int fid, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub->Render(fid,st);
    ret += post;
    return ret;
}

ConfyVal IfThen::Execute(int fid, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) sub->Execute(fid,st,false);
    else {
        v = cond->Execute(fid,st,true);
        sub->Execute(fid,st,v.b);
    }
    return v; 
}

std::string IfThenElse::Render(int fid, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub1->Render(fid,st);
    ret += inter;
    ret += sub2->Render(fid,st);
    ret += post;
    return ret;
}

ConfyVal IfThenElse::Execute(int fid, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) { sub1->Execute(fid,st,false); sub2->Execute(fid,st,false); }
    else {
        v = cond->Execute(fid,st,true);
        // always shadow-execute non-taken branch first, for variable shadowing
        if(v.b) {
            sub2->Execute(fid,st,false);
            sub1->Execute(fid,st,true);
        } else {
            sub1->Execute(fid,st,false);
            sub2->Execute(fid,st,true);
        }
    }
    return v; 
}

std::string Template::Render(int fid, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += temp->Render(fid,st);
    ret += inter;
    ret += out;
    ret += post;
    return ret;
}

ConfyVal Template::Execute(int fid, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) { out=""; }
    else {
        temp->Execute(fid,st,true);
        out = temp->Render(fid,st);
        std::string result;
        /* substitute variable names */
        int pos=0,pos0=0;
        while(pos<out.length()) {
            if(out[pos]=='\\') {
                result.append(out,pos0,pos-pos0); // emit accumulated interval
                ++pos;
                pos0=pos;
                if(out[pos]) ++pos; // skip next character
            } else if(out[pos]=='$') {
                result.append(out,pos0,pos-pos0); // emit accumulated interval
                ++pos; // advance past $
                std::string varname = capture_ident(out.c_str(), NULL, &pos);
                if(out[pos]=='~') ++pos; // advance past ~ for $varname~text
                pos0=pos;
                // append string value of this variable
                if(!st->vars.count(varname))
                    result += "<unknown variable $"+varname+">";
                else
                    result += st->vars[varname].val.s;
            } else ++pos;
        }
        result.append(out,pos0); // emit tail
        out = result;

        temp->Execute(fid,st,false);
    }
    return v; 
}


std::string ExprNode::Render(int fid, ConfyState *st) {
    return source;
}

ConfyVal ExprNode::Execute(int fid, ConfyState *st, bool enable) {
    return root->Eval(fid,st);
}

ConfyVal ExprVar::Eval(int fid, ConfyState *st) {
    if(!st->vars.count(name)) return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
    return st->vars[name].val;
}
ConfyVal ExprLiteral::Eval(int fid, ConfyState *st) {
    return v;
}
ConfyVal ExprOp::Eval(int fid, ConfyState *st) {
    ConfyVal vacc = subexprs[0]->Eval(fid,st);
    for(int i=1;i<subexprs.size();++i) {
        vacc = op(vacc, subexprs[i]->Eval(fid,st), subtypes[i]);
    }
    return vacc;
}
ConfyVal ExprNot::Eval(int fid, ConfyState *st) {
    ConfyVal vsub = sub->Eval(fid,st);
    return boolVal(!vsub.b);
}
ConfyVal ExprNeg::Eval(int fid, ConfyState *st) {
    ConfyVal vsub = sub->Eval(fid,st);
    return vsub.t==T_FLOAT?floatVal(-vsub.f):intVal(-vsub.i);
}

// check for equality, coercing to type of left
ConfyVal ExprEq::Eval(int fid, ConfyState *st) {
    ConfyVal l = left->Eval(fid,st);
    ConfyVal r = right->Eval(fid,st);
    bool res;
    switch(l.t) {
    case T_BOOL: res = l.b == r.b; break;
    case T_INT: res = l.i == r.i; break;
    case T_FLOAT: res = l.f == r.f; break;
    case T_STRING: res = l.s == r.s; break;
    default: res = false;
    }
    return ConfyVal { T_BOOL, res, res?1:0, res?1.0:0.0, res?"true":"false" };
}

std::string VarAssign::Render(int fid, ConfyState *st) {
    return source;
}

ConfyVal VarAssign::Execute(int fid, ConfyState *st, bool enable) {
    if(enable) {
        if(st->vars.count(varname)) {
            st->vars[varname].val.CoerceFrom(expr->Execute(fid,st,enable));
            return st->vars[varname].val;
        } else {
            // TODO: throw error?
        }
    }
    return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
}

std::string Include::Render(int fid, ConfyState *st) {
    return source;
}

ConfyVal Include::Execute(int fid, ConfyState *st, bool enable) {
    // load relative to this file ("absolute" wrt cwd)
    std::string abspath;
    std::filesystem::path fp(fname);
    if(fp.is_absolute()) 
        abspath = fname;
    else {
        abspath = st->files[fid].fpath;
        if(abspath.length()) abspath+="/";
        abspath+=fname;
    }

    if(enable) {
        // this will also execute the file included
        if(st->LoadAndParseFile(abspath))
            return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
    } else {
        // hide all variables from this file if previously loaded
        int i=st->FindFile(abspath);
        for(auto &[k,v] : st->vars) {
            if(v.fl == i)
                v.hidden = true;
        }
    }
    return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
}


