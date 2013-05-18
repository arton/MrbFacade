#pragma once

#define _ALLOW_KEYWORD_MACROS
#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/data.h"
#include "mruby/compile.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/array.h"
#include "mruby/hash.h"

namespace ArtoMuse
{
	namespace MrbFacade
	{
		ref class Mrb;
		public delegate Platform::Object^ CreateObjectEventHandler(Mrb^ sender, Platform::String^ name, const Platform::Array<Platform::Object^>^ args);
		public delegate Platform::Object^ CallObjectEventHandler(Platform::Object^ sender, Platform::String^ name, const Platform::Array<Platform::Object^>^ args);

		public value struct MrbError sealed
		{
			int LineNo;
			Platform::String^ Message;
		};

		bool operator==(const MrbError& e, const MrbError& o)
		{
			return o.LineNo == e.LineNo && o.Message->Equals(e.Message);
		}

		public ref class MrbObject sealed
		{
			friend ref class Mrb;
		public:
			Platform::String^ Inspect();
			virtual ~MrbObject();
		private:
			MrbObject(Mrb^ p, mrb_value v);
			mrb_value value;
			Mrb^ parent;
		};

		public ref class Mrb sealed
		{
			friend ref class MrbObject;
		public:
			Mrb();
			virtual ~Mrb();
			void Export(Platform::String^ name, Platform::Object^ o);
			event CreateObjectEventHandler^ OnCreateObject;
			event CallObjectEventHandler^ OnCallObject;
			bool Parse(Platform::String^ code);
			MrbObject^ Run();
			property Windows::Foundation::Collections::IVector<MrbError>^ Errors
			{
				Windows::Foundation::Collections::IVector<MrbError>^ get() { return errors; }
			}
			inline Platform::Object^ FireCreateObject(Platform::String^ name, const Platform::Array<Platform::Object^>^ args)
			{
				return OnCreateObject(this, name, args);
			}
			inline Platform::Object^ FireInvokeObject(Platform::Object^ obj, Platform::String^ name, const Platform::Array<Platform::Object^>^ args)
			{
				return OnCallObject(obj, name, args);
			}
			inline void RegisterObject(Platform::Object^ object, uint64 id)
			{
				objects->Insert(id, object);
			}
			inline Platform::Object^ GetObject(uint64 id)
			{
				if (!objects->HasKey(id)) return nullptr;
				return objects->Lookup(id);
			}

		private:
			mrbc_context* context;
			mrb_parser_state* parser;
			RClass* rclass;
			RClass* exported;
			mrb_state* mrb;
			mrb_value protect_values;
			void register_rvalue(mrb_value v);
			void unregister_rvalue(mrb_value v);
			int ai;
			char* ToMrbString(Platform::String^);
			Platform::String^ ToPlatformString(const char*, size_t);
			static mrb_value winrt_init(mrb_state* mrb, mrb_value self);
			static mrb_value winrt_missing(mrb_state* mrb, mrb_value self);
			inline static Mrb^ FindThis(mrb_state* p) 
			{
				return mrbmap->Lookup((uint64)p);
			}
			static Platform::Collections::Map<uint64, Mrb^>^ mrbmap;
			Platform::Collections::Map<uint64, Platform::Object^>^ objects;
			Platform::Collections::Vector<MrbError>^ errors;
			bool is_code_block_open();
		};
	}
}