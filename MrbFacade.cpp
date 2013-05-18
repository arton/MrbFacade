// Class1.cpp
#include "pch.h"
#include "MrbFacade.h"

using namespace ArtoMuse::MrbFacade;
using namespace Platform;

Platform::Collections::Map<uint64, Mrb^>^ Mrb::mrbmap = ref new Platform::Collections::Map<uint64, Mrb^>();

mrb_value Mrb::winrt_init(mrb_state* mrb, mrb_value self)
{
	Mrb^ cls = Mrb::FindThis(mrb);
	mrb_value name;
	mrb_value* args;
	unsigned int argc;
	
	Platform::Object^ o = nullptr;
	if (mrb_get_args(mrb, "S*", &name, &args, &argc) > 1)
	{
		Platform::Array<Object^>^ argv = ref new Platform::Array<Object^>(argc);
		for (int i = 0; i < argc; i++)
		{
			if (mrb_fixnum_p(args[i]))
			{
				argv->set(i, mrb_fixnum(args[i]));
			}
			else if (mrb_string_p(args[i]))
			{
				argv->set(i, cls->ToPlatformString(RSTRING_PTR(args[i]), RSTRING_LEN(args[i])));
			}
			else
			{
				argv->set(i, nullptr);
			}
		}
		try
		{
			o = cls->FireCreateObject(cls->ToPlatformString(RSTRING_PTR(name), RSTRING_LEN(name)), argv);
		}
		catch (Platform::Exception^ exc)
		{
		}
	}
	if (o == nullptr)
	{
		mrb_raisef(mrb, E_ARGUMENT_ERROR, "%S not found", name);
	}
	else
	{
		mrb_value mid = mrb_funcall(mrb, self, "object_id", 0);
		cls->RegisterObject(o, mrb_fixnum(mid));
	}
	return self;
}

mrb_value Mrb::winrt_missing(mrb_state* mrb, mrb_value self)
{
	Mrb^ cls = Mrb::FindThis(mrb);
	mrb_value name;
	mrb_value* args;
	unsigned int argc;
	Platform::Object^ o = nullptr;
	if (mrb_get_args(mrb, "o*", &name, &args, &argc) < 1 || !mrb_symbol_p(name))
	{
		mrb_raise(mrb, E_TYPE_ERROR, "name should be a symbol");
	}
	Platform::Array<Object^>^ argv = ref new Platform::Array<Object^>(argc);
	for (int i = 0; i < argc; i++)
	{
		if (mrb_fixnum_p(args[i]))
		{
			argv->set(i, mrb_fixnum(args[i]));
		}
		else if (mrb_string_p(args[i]))
		{
			argv->set(i, cls->ToPlatformString(RSTRING_PTR(args[i]), RSTRING_LEN(args[i])));
		}
		else
		{
			argv->set(i, nullptr);
		}
	}
	try
	{
		mrb_value mid = mrb_funcall(mrb, self, "object_id", 0);
		size_t symlen;
		const char* symp = mrb_sym2name_len(mrb, mrb_symbol(name), &symlen);
		Platform::Object^ obj = cls->GetObject(mrb_fixnum(mid));
		if (obj == nullptr)
		{
			mrb_raise(mrb, E_ARGUMENT_ERROR, "no wrapping object");
		}
		o = cls->FireInvokeObject(obj, cls->ToPlatformString(symp, symlen), argv);
	}
	catch (Platform::Exception^ exc)
	{
		mrb_raisef(mrb, E_ARGUMENT_ERROR, "failed to invoke %S", name);
	}
	if (o == nullptr)
	{
		return mrb_nil_value();
	}
	return self;
}

MrbObject::MrbObject(Mrb^ p, mrb_value v) 
	: parent(p), value(v)
{
	p->register_rvalue(v);
}

MrbObject::~MrbObject()
{
	parent->unregister_rvalue(value);
}

Mrb::Mrb()
	: parser(NULL)
{
	mrb = mrb_open();
	mrbmap->Insert((uint64)mrb, this);
	protect_values = mrb_hash_new(mrb);
	mrb_gc_protect(mrb, protect_values);
	objects = ref new Platform::Collections::Map<uint64, Platform::Object^>();
	errors = ref new Platform::Collections::Vector<MrbError>();
	rclass = mrb_define_class(mrb, "WinRT", mrb->object_class);
	mrb_define_method(mrb, rclass, "initialize", winrt_init, ARGS_REQ(1));
	mrb_define_method(mrb, rclass, "method_missing", winrt_missing, ARGS_ANY());
	exported = mrb_define_class(mrb, "WinRTProxy", mrb->object_class);
	mrb_define_method(mrb, exported, "method_missing", winrt_missing, ARGS_ANY());
	context = mrbc_context_new(mrb);
	context->capture_errors = 1;
	ai = mrb_gc_arena_save(mrb);
}

Mrb::~Mrb()
{
	mrbc_context_free(mrb, context);
	mrb_close(mrb);
}

//
// code from mruby/tools/mirb/mirb.c
//
/* Guess if the user might want to enter more
 * or if he wants an evaluation of his code now */
bool Mrb::is_code_block_open()
{
  bool code_block_open = false;

  /* check for heredoc */
  if (parser->parsing_heredoc != NULL) return false;
  if (parser->heredoc_end_now) {
    parser->heredoc_end_now = false;
    return false;
  }

  /* check if parser error are available */
  if (0 < parser->nerr) {
    const char *unexpected_end = "syntax error, unexpected $end";
    const char *message = parser->error_buffer[0].message;
	MrbError me;
	me.LineNo = parser->error_buffer[0].lineno;
	me.Message = ToPlatformString(message, strlen(message));
	errors->Append(me);

    /* a parser error occur, we have to check if */
    /* we need to read one more line or if there is */
    /* a different issue which we have to show to */
    /* the user */

    if (strncmp(message, unexpected_end, strlen(unexpected_end)) == 0) {
      code_block_open = true;
    }
    else if (strcmp(message, "syntax error, unexpected keyword_end") == 0) {
      code_block_open = false;
    }
    else if (strcmp(message, "syntax error, unexpected tREGEXP_BEG") == 0) {
      code_block_open = false;
    }
    return code_block_open;
  }

  /* check for unterminated string */
  if (parser->lex_strterm) return true;

  switch (parser->lstate) {

  /* all states which need more code */

  case EXPR_BEG:
    /* an expression was just started, */
    /* we can't end it like this */
    code_block_open = true;
    break;
  case EXPR_DOT:
    /* a message dot was the last token, */
    /* there has to come more */
    code_block_open = true;
    break;
  case EXPR_CLASS:
    /* a class keyword is not enough! */
    /* we need also a name of the class */
    code_block_open = true;
    break;
  case EXPR_FNAME:
    /* a method name is necessary */
    code_block_open = true;
    break;
  case EXPR_VALUE:
    /* if, elsif, etc. without condition */
    code_block_open = true;
    break;

  /* now all the states which are closed */

  case EXPR_ARG:
    /* an argument is the last token */
    code_block_open = false;
    break;

  /* all states which are unsure */

  case EXPR_CMDARG:
    break;
  case EXPR_END:
    /* an expression was ended */
    break;
  case EXPR_ENDARG:
    /* closing parenthese */
    break;
  case EXPR_ENDFN:
    /* definition end */
    break;
  case EXPR_MID:
    /* jump keyword like break, return, ... */
    break;
  case EXPR_MAX_STATE:
    /* don't know what to do with this token */
    break;
  default:
    /* this state is unexpected! */
    break;
  }

  return code_block_open;
}

void Mrb::Export(Platform::String^ name, Platform::Object^ o)
{
	mrb_value obj = mrb_instance_new(mrb, mrb_obj_value(exported));
	mrb_value mid = mrb_funcall(mrb, obj, "object_id", 0);
	RegisterObject(o, mrb_fixnum(mid));
	char* mname = ToMrbString(name);
	mrb_define_global_const(mrb, mname, obj);
	mrb_free(mrb, mname);
}

bool Mrb::Parse(Platform::String^ code)
{
	errors->Clear();
	char* pc = ToMrbString(code);
	parser = mrb_parser_new(mrb);
	parser->s = pc;
	parser->send = pc + strlen(pc);
	parser->lineno = 1;
	mrb_parser_parse(parser, context);
	bool ret = is_code_block_open();
	if (ret)
	{
		mrb_parser_free(parser);
		parser = NULL;
	}
	return ret;
}

MrbObject^ Mrb::Run()
{
	if (!parser) throw ref new Platform::NullReferenceException("no previous parse");
	int n = mrb_generate_code(mrb, parser);
	mrb_value result = mrb_run(mrb, 
		mrb_proc_new(mrb, mrb->irep[n]),
		mrb_top_self(mrb));
	if (mrb->exc)
	{
		result = mrb_obj_value(mrb->exc);
		mrb->exc = 0;
	}
	mrb_parser_free(parser);
	parser = NULL;
	mrb_gc_arena_restore(mrb, ai);
	return ref new MrbObject(this, result);
}

Platform::String^ MrbObject::Inspect()
{
	mrb_value result = (!mrb_respond_to(parent->mrb, value, mrb_intern(parent->mrb, "inspect")))
		? mrb_any_to_s(parent->mrb, value) 
		: mrb_funcall(parent->mrb, value, "inspect", 0);
	return parent->ToPlatformString(RSTRING_PTR(result), RSTRING_LEN(result));
}

Platform::String^ Mrb::ToPlatformString(const char* p, size_t len)
{
	wchar_t* wptr = (wchar_t*)mrb_calloc(mrb, sizeof(wchar_t), len + 1);
	int cb = MultiByteToWideChar(CP_UTF8, 0, p, len, wptr, (len + 1) * sizeof(wchar_t));
	*(wptr + cb) = 0;
	Platform::String^ ret = ref new String(wptr);
	mrb_free(mrb, wptr);
	return ret;
}

char* Mrb::ToMrbString(Platform::String^ str)
{
	int cb = WideCharToMultiByte(CP_UTF8, 0, str->Data(), str->Length(), NULL, 0, NULL, NULL);
	char* buf = (char *)mrb_malloc(mrb, cb + 1);
	WideCharToMultiByte(CP_UTF8, 0, str->Data(), str->Length(), buf, cb, NULL, NULL);
	buf[cb] = 0;
	return buf;
}

void Mrb::register_rvalue(mrb_value v)
{
	if (mrb_fixnum_p(v) || mrb_float_p(v) || mrb_nil_p(v) || mrb_symbol_p(v)) return;
	mrb_hash_set(mrb, protect_values, v, mrb_nil_value());
}

void Mrb::unregister_rvalue(mrb_value v)
{
	if (mrb_fixnum_p(v) || mrb_float_p(v) || mrb_nil_p(v) || mrb_symbol_p(v)) return;
	mrb_hash_delete_key(mrb, protect_values, v);
}


