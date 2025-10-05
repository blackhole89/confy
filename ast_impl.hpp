// AST functions implementations

std::string Seq::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    for(auto n : children) 
        ret += n->Render(f, st);
    return ret;
}

ConfyVal Seq::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal ret;
    for(auto n : children)
        ret = n->Execute(f,st,enable);
    return ret;
}

std::string VarDef::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += v.val.Render();
    ret += post;
    return ret;
}

ConfyVal VarDef::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    if(!st->vars.count(name)) {
        st->vars[name] = v;
        st->vars[name].fl = f;
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

std::string SourceBlock::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    if(bType == B_INERT_LINE) {
        ret = lineify(contents, f->setup.line);
    } else if(bType == B_INERT_BLOCK) {
        ret += f->setup.block_start;
        ret += contents;
        ret += f->setup.block_end;
    } else ret = contents;
    return ret;
}

ConfyVal SourceBlock::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    if(bType == B_META_CHAFF) return { T_BOOL, true, 1, 1.0, "true" };

    if(enable) bType=B_ACTIVE;
    else if(f->setup.line.length() && contents.find('\n')==(contents.length()-1)) bType=B_INERT_LINE;
    else if(f->setup.block_start.length()) bType=B_INERT_BLOCK;
    else bType=B_INERT_LINE;

    return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
}

std::string IfThen::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub->Render(f,st);
    ret += post;
    return ret;
}

ConfyVal IfThen::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) sub->Execute(f,st,false);
    else {
        v = cond->Execute(f,st,true);
        sub->Execute(f,st,v.b);
    }
    return v; 
}

std::string IfThenElse::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub1->Render(f,st);
    ret += inter;
    ret += sub2->Render(f,st);
    ret += post;
    return ret;
}

ConfyVal IfThenElse::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) { sub1->Execute(f,st,false); sub2->Execute(f,st,false); }
    else {
        v = cond->Execute(f,st,true);
        sub1->Execute(f,st,v.b);
        sub2->Execute(f,st,!v.b);
    }
    return v; 
}

std::string Template::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += temp->Render(f,st);
    ret += inter;
    ret += out;
    ret += post;
    return ret;
}

ConfyVal Template::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) {  }
    else {
        temp->Execute(f,st,true);
        out = temp->Render(f,st);
        std::string result;
        /* substitute variable names */
        int pos=0,pos0=0;
        while(pos<out.length()) {
            if(out[pos]=='\\') {
                result.append(out,pos0,pos-pos0); // emit accumulated interval
                ++pos; // skip next character
                pos0=pos;
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

        temp->Execute(f,st,false);
    }
    return v; 
}


std::string ExprNode::Render(ConfyFile *f, ConfyState *st) {
    return source;
}

ConfyVal ExprNode::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    return root->Eval(f,st);
}

ConfyVal ExprVar::Eval(ConfyFile *f, ConfyState *st) {
    if(!st->vars.count(name)) return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
    return st->vars[name].val;
}
ConfyVal ExprLiteral::Eval(ConfyFile *f, ConfyState *st) {
    return v;
}
ConfyVal ExprOp::Eval(ConfyFile *f, ConfyState *st) {
    ConfyVal vacc = subexprs[0]->Eval(f,st);
    for(int i=1;i<subexprs.size();++i) {
        vacc = op(vacc, subexprs[i]->Eval(f,st), subtypes[i]);
    }
    return vacc;
}
ConfyVal ExprNot::Eval(ConfyFile *f, ConfyState *st) {
    ConfyVal vsub = sub->Eval(f,st);
    return boolVal(!vsub.b);
}
ConfyVal ExprNeg::Eval(ConfyFile *f, ConfyState *st) {
    ConfyVal vsub = sub->Eval(f,st);
    return vsub.t==T_FLOAT?floatVal(-vsub.f):intVal(-vsub.i);
}

// check for equality, coercing to type of left
ConfyVal ExprEq::Eval(ConfyFile *f, ConfyState *st) {
    ConfyVal l = left->Eval(f,st);
    ConfyVal r = right->Eval(f,st);
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

std::string VarAssign::Render(ConfyFile *f, ConfyState *st) {
    return source;
}

ConfyVal VarAssign::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    if(enable) {
        if(st->vars.count(varname)) {
            st->vars[varname].val = expr->Execute(f,st,enable);
            return st->vars[varname].val;
        } else {
            // TODO: throw error?
        }
    }
    return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
}


