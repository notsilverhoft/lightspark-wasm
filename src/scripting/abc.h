/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef SCRIPTING_ABC_H
#define SCRIPTING_ABC_H 1

#include "compat.h"
#include <cstddef>
#include "parsing/tags.h"
#include "parsing/streams.h"
#include "logger.h"
#include <vector>
#include <deque>
#include <map>
#include <set>
#include "swf.h"
#include "scripting/abcutils.h"
#include "scripting/abctypes.h"
#include "scripting/flash/system/flashsystem.h"

namespace llvm {
	class ExecutionEngine;
#ifdef LLVM_36
namespace legacy {
	class FunctionPassManager;
}
#else
	class FunctionPassManager;
#endif
	class FunctionType;
	class Function;
	class Module;
	class Type;
	class Value;
	class LLVMContext;
}

namespace lightspark
{
struct block_info;
#ifdef LLVM_28
typedef const llvm::Type* LLVMTYPE;
#else
typedef llvm::Type* LLVMTYPE;
#endif

bool isVmThread();

std::ostream& operator<<(std::ostream& o, const block_info& b);

typedef std::pair<llvm::Value*, STACK_TYPE> stack_entry;
inline stack_entry make_stack_entry(llvm::Value* v, STACK_TYPE t)
{
	return std::make_pair(v, t);
}

class method_info
{
friend std::istream& operator>>(std::istream& in, method_info& v);
friend struct block_info;
friend class SyntheticFunction;
private:
	struct method_info_simple info;

	typedef std::vector<std::pair<int, STACK_TYPE> > static_stack_types_vector;
	//Helper function to sync only part of the static stack to the memory
	void consumeStackForRTMultiname(static_stack_types_vector& stack, int multinameIndex) const;

	//Helper function to add a basic block
	void addBlock(std::map<unsigned int,block_info>& blocks, unsigned int ip, const char* blockName);
	std::pair<unsigned int, STACK_TYPE> popTypeFromStack(static_stack_types_vector& stack, unsigned int localIp) const;
	llvm::FunctionType* synt_method_prototype(llvm::ExecutionEngine* ex);
	llvm::Function* llvmf;

	// Wrapper needed because llvm::IRBuilder is a template, cannot forward declare
	struct BuilderWrapper;
	//Does analysis on function code to find optimization chances
	void doAnalysis(std::map<unsigned int,block_info>& blocks, BuilderWrapper& builderWrapper);

public:
#ifdef PROFILING_SUPPORT
	std::map<method_info*,uint64_t> profCalls;
	std::vector<uint64_t> profTime;
	tiny_string profName;
	bool validProfName;
#endif

	SyntheticFunction::synt_function f;
	ABCContext* context;
	method_body_info* body;
	SyntheticFunction::synt_function synt_method(SystemState* sys);
	bool needsArgs() { return info.needsArgs(); }
	bool needsActivation() { return info.needsActivation(); }
	bool needsRest() { return info.needsRest(); }
	bool hasOptional() { return info.hasOptional(); }
	bool hasDXNS() { return info.hasDXNS(); }
	bool hasParamNames() { return info.hasParamNames(); }
	ASObject* getOptional(unsigned int i);
	uint32_t numOptions() { return info.option_count; }
	uint32_t numArgs() { return info.param_count; }
	const multiname* paramTypeName(uint32_t i) const;
	const multiname* returnTypeName() const;

	std::vector<const Type*> paramTypes;
	const Type* returnType;
	bool hasExplicitTypes;
	method_info():
		llvmf(NULL),
#ifdef PROFILING_SUPPORT
		profTime(0),
		validProfName(false),
#endif
		f(NULL),context(NULL),body(NULL),returnType(NULL),hasExplicitTypes(false)
	{
	}
};

struct opcode_handler
{
	const char* name;
	void* addr;
};

enum ARGS_TYPE { ARGS_OBJ_OBJ=0, ARGS_OBJ_INT, ARGS_OBJ, ARGS_INT, ARGS_OBJ_OBJ_INT, ARGS_NUMBER, ARGS_OBJ_NUMBER,
	ARGS_BOOL, ARGS_INT_OBJ, ARGS_NONE, ARGS_NUMBER_OBJ, ARGS_INT_INT, ARGS_CONTEXT, ARGS_CONTEXT_INT, ARGS_CONTEXT_INT_INT,
	ARGS_CONTEXT_INT_INT_INT, ARGS_CONTEXT_INT_INT_INT_BOOL, ARGS_CONTEXT_OBJ_OBJ_INT, ARGS_CONTEXT_OBJ, ARGS_CONTEXT_OBJ_OBJ,
	ARGS_CONTEXT_OBJ_OBJ_OBJ, ARGS_OBJ_OBJ_OBJ_INT, ARGS_OBJ_OBJ_OBJ };

struct typed_opcode_handler
{
	const char* name;
	void* addr;
	ARGS_TYPE type;
};

class ABCContext
{
friend class ABCVm;
friend class method_info;
public:
	_R<RootMovieClip> root;

	method_info* get_method(unsigned int m);
	uint32_t getString(unsigned int s) const;
	//Qname getQname(unsigned int m, call_context* th=NULL) const;
	static multiname* s_getMultiname(ABCContext*, ASObject* rt1, ASObject* rt2, int m);
	static multiname* s_getMultiname_i(call_context*, uint32_t i , int m);
	static multiname* s_getMultiname_d(call_context*, number_t i , int m);
	ASObject* getConstant(int kind, int index);
	u16 minor;
	u16 major;
	cpool_info constant_pool;
	u30 method_count;
	std::vector<method_info, reporter_allocator<method_info>> methods;
	u30 metadata_count;
	std::vector<metadata_info, reporter_allocator<metadata_info>> metadata;
	u30 class_count;
	std::vector<instance_info, reporter_allocator<instance_info>> instances;
	std::vector<class_info, reporter_allocator<class_info>> classes;
	u30 script_count;
	std::vector<script_info, reporter_allocator<script_info>> scripts;
	u30 method_body_count;
	std::vector<method_body_info, reporter_allocator<method_body_info>> method_body;
	//Base for namespaces in this context
	uint32_t namespaceBaseId;

	std::vector<bool> hasRunScriptInit;
	/**
		Construct and insert in the a object a given trait
		@param obj the tarhget object
		@param t the trait to build
		@param isBorrowed True if we're building a trait on behalf of an object, False otherwise
		@param deferred_initialization A pointer to a function that can be used to build the given trait later
	*/
	void buildTrait(ASObject* obj, const traits_info* t, bool isBorrowed, int scriptid=-1, bool checkExisting=true);
	void runScriptInit(unsigned int scriptid, ASObject* g);

	void linkTrait(Class_base* obj, const traits_info* t);
	void getOptionalConstant(const option_detail& opt);

	/* This function determines how many stack values are needed for
	 * resolving the multiname at index mi
	 */
	inline int getMultinameRTData(int mi) const
	{
		if(mi==0)
			return 0;
	
		const multiname_info* m=&constant_pool.multinames[mi];
		switch(m->kind)
		{
			case 0x07: //QName
			case 0x0d: //QNameA
			case 0x09: //Multiname
			case 0x0e: //MultinameA
			case 0x1d: //Templated name
				return 0;
			case 0x0f: //RTQName
			case 0x10: //RTQNameA
			case 0x1b: //MultinameL
			case 0x1c: //MultinameLA
				return 1;
			case 0x11: //RTQNameL
			case 0x12: //RTQNameLA
				return 2;
			default:
				LOG(LOG_ERROR,_("getMultinameRTData not yet implemented for this kind ") << m->kind);
				throw UnsupportedException("kind not implemented for getMultinameRTData");
		}
	}
	
	multiname* getMultiname(unsigned int m, call_context* th);
	multiname* getMultinameImpl(ASObject* rt1, ASObject* rt2, unsigned int m);
	void buildInstanceTraits(ASObject* obj, int class_index);
	ABCContext(_R<RootMovieClip> r, std::istream& in, ABCVm* vm) DLL_PUBLIC;
	void exec(bool lazy);

	bool isinstance(ASObject* obj, multiname* name);

#ifdef PROFILING_SUPPORT
	void dumpProfilingData(std::ostream& f) const;
#endif
};

struct BasicBlock;
struct InferenceData;

class ABCVm
{
friend class ABCContext;
friend class method_info;
private:
	std::vector<ABCContext*> contexts;
	SystemState* m_sys;
	Thread* t;
	enum STATUS { CREATED=0, STARTED, TERMINATED };
	STATUS status;

	llvm::Module* module;

	void registerClasses();

	void registerFunctions();
	//Interpreted AS instructions
	//If you change a definition here, update the opcode_table_* entry in abc_codesynth
	static bool hasNext2(call_context* th, int n, int m); 
	template<class T>
	static void loadIntN(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		uint32_t addr=arg1->toUInt();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		T ret=appDomain->readFromDomainMemory<T>(addr);
		th->runtime_stack_push(abstract_i(arg1->getSystemState(),ret));
		arg1->decRef();
	}
	template<class T>
	static void storeIntN(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		ASObject* arg2=th->runtime_stack_pop();
		uint32_t addr=arg1->toUInt();
		arg1->decRef();
		int32_t val=arg2->toInt();
		arg2->decRef();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		appDomain->writeToDomainMemory<T>(addr, val);
	}
	static void loadFloat(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		float addr=arg1->toNumber();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		number_t ret=appDomain->readFromDomainMemory<float>(addr);
		th->runtime_stack_push(abstract_d(arg1->getSystemState(),ret));
		arg1->decRef();
	}

	static void loadDouble(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		double addr=arg1->toNumber();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		number_t ret=appDomain->readFromDomainMemory<double>(addr);
		th->runtime_stack_push(abstract_d(arg1->getSystemState(),ret));
		arg1->decRef();
	}
	static void storeFloat(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		ASObject* arg2=th->runtime_stack_pop();
		number_t addr=arg1->toNumber();
		arg1->decRef();
		float val=(float)arg2->toNumber();
		arg2->decRef();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		appDomain->writeToDomainMemory<float>(addr, val);
	}
	static void storeDouble(call_context* th)
	{
		ASObject* arg1=th->runtime_stack_pop();
		ASObject* arg2=th->runtime_stack_pop();
		number_t addr=arg1->toNumber();
		arg1->decRef();
		double val=arg2->toNumber();
		arg2->decRef();
		_R<ApplicationDomain> appDomain = getCurrentApplicationDomain(th);
		appDomain->writeToDomainMemory<double>(addr, val);
	}

	static void callStatic(call_context* th, int n, int m, method_info** called_mi, bool keepReturn);
	static void callSuper(call_context* th, int n, int m, method_info** called_mi, bool keepReturn);
	static void callProperty(call_context* th, int n, int m, method_info** called_mi, bool keepReturn);
	static void callImpl(call_context* th, ASObject* f, ASObject* obj, ASObject** args, int m, method_info** called_mi, bool keepReturn);
	static void constructProp(call_context* th, int n, int m); 
	static void setLocal(int n); 
	static void setLocal_int(int n,int v); 
	static void setLocal_obj(int n,ASObject* v);
	static void getLocal(ASObject* o, int n); 
	static void getLocal_short(int n); 
	static void getLocal_int(int n, int v); 
	static void newObject(call_context* th, int n); 
	static void getDescendants(call_context* th, int n); 
	static ASObject* newCatch(call_context* th, int n); 
	static void jump(int offset); 
	static bool ifEq(ASObject*, ASObject*); 
	static bool ifStrictEq(ASObject*, ASObject*); 
	static bool ifNE(ASObject*, ASObject*); 
	static bool ifNE_oi(ASObject*, int32_t);
	static bool ifLT(ASObject*, ASObject*); 
	static bool ifGT(ASObject*, ASObject*); 
	static bool ifLE(ASObject*, ASObject*); 
	static bool ifLT_oi(ASObject*, int32_t);
	static bool ifLT_io(int32_t, ASObject*);
	static bool ifNLT(ASObject*, ASObject*); 
	static bool ifNGT(ASObject*, ASObject*); 
	static bool ifNGE(ASObject*, ASObject*); 
	static bool ifGE(ASObject*, ASObject*); 
	static bool ifNLE(ASObject*, ASObject*); 
	static bool ifStrictNE(ASObject*, ASObject*); 
	static bool ifFalse(ASObject*); 
	static bool ifTrue(ASObject*); 
	static ASObject* getSlot(ASObject* th, int n); 
	static void setSlot(ASObject*, ASObject*, int n); 
	static void kill(int n); 
	static ASObject* pushString(call_context* th, int n); 
	static bool getLex(call_context* th, int n); 
	static ASObject* getScopeObject(call_context* th, int n); 
	static bool deleteProperty(ASObject* obj, multiname* name);
	static void initProperty(ASObject* obj, ASObject* val, multiname* name);
	static void newClass(call_context* th, int n);
	static void newArray(call_context* th, int n); 
	static ASObject* findPropStrict(call_context* th, multiname* name);
	static ASObject* findPropStrictCache(call_context* th, memorystream& code);
	static ASObject* findProperty(call_context* th, multiname* name);
	static int32_t pushByte(intptr_t n);
	static int32_t pushShort(intptr_t n);
	static void pushInt(call_context* th, int32_t val);
	static void pushUInt(call_context* th, uint32_t val);
	static void pushDouble(call_context* th, double val);
	static void incLocal_i(call_context* th, int n);
	static void incLocal(call_context* th, int n);
	static void decLocal_i(call_context* th, int n);
	static void decLocal(call_context* th, int n);
	static void coerce(call_context* th, int n);
	static void checkDeclaredTraits(ASObject* obj);
	static ASObject* getProperty(ASObject* obj, multiname* name);
	static int32_t getProperty_i(ASObject* obj, multiname* name);
	static void setProperty(ASObject* value,ASObject* obj, multiname* name);
	static void setProperty_i(int32_t value,ASObject* obj, multiname* name);
	static void call(call_context* th, int n, method_info** called_mi);
	static void constructSuper(call_context* th, int n);
	static void construct(call_context* th, int n);
	static void constructGenericType(call_context* th, int n);
	static ASObject* newFunction(call_context* th, int n);
	static void setSuper(call_context* th, int n);
	static void getSuper(call_context* th, int n);
	static void pushScope(call_context* obj);
	static void pushWith(call_context* th);
	static ASObject* pushNull();
	static ASObject* pushUndefined();
	static ASObject* pushNaN();
	static bool pushFalse();
	static bool pushTrue();
	static void dup();
	static bool in(ASObject*,ASObject*);
	static bool _not(ASObject*);
	static bool equals(ASObject*,ASObject*);
	static number_t negate(ASObject*);
	static int32_t negate_i(ASObject*);
	static void pop();
	static ASObject* typeOf(ASObject*);
	static void _throw(call_context* th);
	static ASObject* asType(ABCContext* context, ASObject* obj, multiname* name);
	static ASObject* asTypelate(ASObject* type, ASObject* obj);
	static bool isTypelate(ASObject* type, ASObject* obj);
	static bool isType(ABCContext* context, ASObject* obj, multiname* name);
	static void swap();
	static ASObject* add(ASObject*,ASObject*);
	static int32_t add_i(ASObject*,ASObject*);
	static ASObject* add_oi(ASObject*,int32_t);
	static ASObject* add_od(ASObject*,number_t);
	static int32_t bitAnd(ASObject*,ASObject*);
	static int32_t bitNot(ASObject*);
	static int32_t bitAnd_oi(ASObject* val1, int32_t val2);
	static int32_t bitOr(ASObject*,ASObject*);
	static int32_t bitOr_oi(ASObject*,int32_t);
	static int32_t bitXor(ASObject*,ASObject*);
	static int32_t rShift(ASObject*,ASObject*);
	static uint32_t urShift(ASObject*,ASObject*);
	static uint32_t urShift_io(uint32_t,ASObject*);
	static int32_t lShift(ASObject*,ASObject*);
	static int32_t lShift_io(uint32_t,ASObject*);
	static number_t multiply(ASObject*,ASObject*);
	static int32_t multiply_i(ASObject*,ASObject*);
	static number_t multiply_oi(ASObject*, int32_t);
	static number_t divide(ASObject*,ASObject*);
	static number_t modulo(ASObject*,ASObject*);
	static number_t subtract(ASObject*,ASObject*);
	static int32_t subtract_i(ASObject*,ASObject*);
	static number_t subtract_oi(ASObject*, int32_t);
	static number_t subtract_io(int32_t, ASObject*);
	static number_t subtract_do(number_t, ASObject*);
	static void popScope(call_context* th);
	static ASObject* newActivation(call_context* th, method_info* mi);
	static ASObject* coerce_s(ASObject*);
	static ASObject* checkfilter(ASObject*);
	static void coerce_a();
	static void label();
	static void lookupswitch();
	static int32_t convert_i(ASObject*);
	static int64_t convert_di(ASObject*);
	static uint32_t convert_u(ASObject*);
	static number_t convert_d(ASObject*);
	static ASObject* convert_s(ASObject*);
	static bool convert_b(ASObject*);
	static bool greaterThan(ASObject*,ASObject*);
	static bool greaterEquals(ASObject*,ASObject*);
	static bool lessEquals(ASObject*,ASObject*);
	static bool lessThan(ASObject*,ASObject*);
	static ASObject* nextName(ASObject* index, ASObject* obj);
	static ASObject* nextValue(ASObject* index, ASObject* obj);
	static uint32_t increment_i(ASObject*);
	static uint64_t increment_di(ASObject*);
	static number_t increment(ASObject*);
	static number_t decrement(ASObject*);
	static uint32_t decrement_i(ASObject*);
	static uint64_t decrement_di(ASObject*);
	static bool strictEquals(ASObject*,ASObject*);
	static ASObject* esc_xattr(ASObject* o);
	static ASObject* esc_xelem(ASObject* o);
	static bool instanceOf(ASObject* value, ASObject* type);
	static Namespace* pushNamespace(call_context* th, int n);
	static void dxns(call_context* th, int n);
	static void dxnslate(call_context* th, ASObject* value);
	//Utility
	static void not_impl(int p);
	static void wrong_exec_pos();

	//Internal utilities
	static void method_reset(method_info* th);

	static void SetAllClassLinks();
	static void AddClassLinks(Class_base* target);
	static bool newClassRecursiveLink(Class_base* target, Class_base* c);
	static ASObject* constructFunction(call_context* th, IFunction* f, ASObject** args, int argslen);
	void parseRPCMessage(_R<ByteArray> message, _NR<ASObject> client, _NR<Responder> responder);

	//Opcode tables
	void register_table(LLVMTYPE ret_type,typed_opcode_handler* table, int table_len);
	static opcode_handler opcode_table_args_pointer_2int[];
	static opcode_handler opcode_table_args_pointer_number_int[];
	static opcode_handler opcode_table_args3_pointers[];
	static typed_opcode_handler opcode_table_uint32_t[];
	static typed_opcode_handler opcode_table_number_t[];
	static typed_opcode_handler opcode_table_void[];
	static typed_opcode_handler opcode_table_voidptr[];
	static typed_opcode_handler opcode_table_bool_t[];


	//Synchronization
	Mutex event_queue_mutex;
	Cond sem_event_cond;

	//Event handling
	volatile bool shuttingdown;
	typedef std::pair<_NR<EventDispatcher>,_R<Event>> eventType;
	std::deque<eventType, reporter_allocator<eventType>> events_queue;
	void handleEvent(std::pair<_NR<EventDispatcher>,_R<Event> > e);
	void handleFrontEvent();
	void signalEventWaiters();
	void buildClassAndInjectBase(const std::string& s, _R<RootMovieClip> base);
	Class_inherit* findClassInherit(const std::string& s, RootMovieClip* r);

	//Profiling support
	static uint64_t profilingCheckpoint(uint64_t& startTime);
	// The base to assign to the next loaded context
	ATOMIC_INT32(nextNamespaceBase);

	typedef void (*abc_function)(const SyntheticFunction*, call_context* ,memorystream&);
	
	static void abc_bkpt(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x01
	static void abc_nop(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_throw(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getSuper(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setSuper(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_dxns(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_dxnslate(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_kill(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_label(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifnlt(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifnle(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifngt(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifnge(const SyntheticFunction* function, call_context* context,memorystream& code);
	
	static void abc_jump(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x10
	static void abc_iftrue(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_iffalse(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifeq(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifne(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_iflt(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifle(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifgt(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifge(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifstricteq(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_ifstrictne(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lookupswitch(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushwith(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_popscope(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_nextname(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_pushnull(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x20
	static void abc_pushundefined(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_nextvalue(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushbyte(const SyntheticFunction* function, call_context* context, memorystream &code);
	static void abc_pushshort(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushtrue(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushfalse(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushnan(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pop(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_dup(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_swap(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushstring(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushint(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushuint(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_pushdouble(const SyntheticFunction* function, call_context* context, memorystream &code);

	static void abc_pushScope(const SyntheticFunction* function, call_context* context, memorystream &code);// 0x30
	static void abc_pushnamespace(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_hasnext2(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_li8(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_li16(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_li32(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lf32(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lf64(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_si8(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_si16(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_si32(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_sf32(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_sf64(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_newfunction(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x40
	static void abc_call(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_construct(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_callstatic(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_callsuper(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_callproperty(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_returnvoid(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_returnvalue(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_constructsuper(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_constructprop(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_callsupervoid(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_callpropvoid(const SyntheticFunction* function, call_context* context,memorystream& code);
	
	static void abc_sxi1(const SyntheticFunction* function, call_context* context,memorystream& code); // 0x50
	static void abc_sxi8(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_sxi16(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_constructgenerictype(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_newobject(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_newarray(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_newactivation(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_newclass(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getdescendants(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_newcatch(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_findpropstrict(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_findproperty(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_finddef(const SyntheticFunction* function, call_context* context,memorystream& code);
	
	static void abc_getlex(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x60
	static void abc_setproperty(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getlocal(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setlocal(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getglobalscope(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getscopeobject(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getProperty(const SyntheticFunction* function, call_context* context, memorystream &code);
	static void abc_initproperty(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_deleteproperty(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getslot(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setslot(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getglobalSlot(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setglobalSlot(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_convert_s(const SyntheticFunction* function, call_context* context,memorystream& code);// 0x70
	static void abc_esc_xelem(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_esc_xattr(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_convert_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_convert_u(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_convert_d(const SyntheticFunction* function, call_context* context, memorystream &code);
	static void abc_convert_b(const SyntheticFunction* function, call_context* context, memorystream &code);
	static void abc_convert_o(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_checkfilter(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_coerce(const SyntheticFunction* function, call_context* context,memorystream& code); // 0x80
	static void abc_coerce_a(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_coerce_s(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_astype(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_astypelate(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_negate(const SyntheticFunction* function, call_context* context,memorystream& code); // 0x90
	static void abc_increment(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_inclocal(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_decrement(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_declocal(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_typeof(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_not(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_bitnot(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_add(const SyntheticFunction* function, call_context* context,memorystream& code); //0xa0
	static void abc_subtract(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_multiply(const SyntheticFunction* function, call_context* context, memorystream &code);
	static void abc_divide(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_modulo(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lshift(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_rshift(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_urshift(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_bitand(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_bitor(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_bitxor(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_equals(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_strictequals(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lessthan(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_lessequals(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_greaterthan(const SyntheticFunction* function, call_context* context,memorystream& code);
	
	static void abc_greaterequals(const SyntheticFunction* function, call_context* context,memorystream& code);// 0xb0
	static void abc_instanceof(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_istype(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_istypelate(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_in(const SyntheticFunction* function, call_context* context,memorystream& code);
	
	static void abc_increment_i(const SyntheticFunction* function, call_context* context,memorystream& code); // 0xc0
	static void abc_decrement_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_inclocal_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_declocal_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_negate_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_add_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_subtract_i(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_multiply_i(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_getlocal_0(const SyntheticFunction* function, call_context* context,memorystream& code); // 0xd0
	static void abc_getlocal_1(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getlocal_2(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_getlocal_3(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setlocal_0(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setlocal_1(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setlocal_2(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_setlocal_3(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_debug(const SyntheticFunction* function, call_context* context,memorystream& code); // 0xef

	static void abc_debugline(const SyntheticFunction* function, call_context* context,memorystream& code); //0xf0
	static void abc_debugfile(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_bkptline(const SyntheticFunction* function, call_context* context,memorystream& code);
	static void abc_timestamp(const SyntheticFunction* function, call_context* context,memorystream& code);

	static void abc_invalidinstruction(const SyntheticFunction* function, call_context* context,memorystream& code);

	static abc_function abcfunctions[];
public:
	call_context* currentCallContext;

	MemoryAccount* vmDataMemory;

	llvm::ExecutionEngine* ex;
#ifdef LLVM_36
	llvm::legacy::FunctionPassManager* FPM;
#else
	llvm::FunctionPassManager* FPM;
#endif
	llvm::LLVMContext& llvm_context();

	ABCVm(SystemState* s, MemoryAccount* m) DLL_PUBLIC;
	/**
		Destroys the VM

		@pre shutdown must have been called
	*/
	~ABCVm();
	/**
	  	Start the VM thread
	*/
	void start() DLL_PUBLIC;
	void finalize();
	static void Run(ABCVm* th);
	static void executeFunction(const SyntheticFunction* function, call_context* context);
	static void preloadFunction(const SyntheticFunction* function);
	static ASObject* executeFunctionFast(const SyntheticFunction* function, call_context* context, ASObject *caller);
	static void optimizeFunction(SyntheticFunction* function);
	static void verifyBranch(std::set<uint32_t>& pendingBlock,std::map<uint32_t,BasicBlock>& basicBlocks,
			int oldStart, int here, int offset, int code_len);
	static void writeBranchAddress(std::map<uint32_t,BasicBlock>& basicBlocks, int here, int offset, std::ostream& out);
	static void writeInt32(std::ostream& out, int32_t val);
	static void writeDouble(std::ostream& out, double val);
	static void writePtr(std::ostream& out, const void* val);

	static InferenceData earlyBindGetLex(std::ostream& out, const SyntheticFunction* f,
			const std::vector<InferenceData>& scopeStack, const multiname* name, uint32_t name_index);
	static InferenceData earlyBindFindPropStrict(std::ostream& out, const SyntheticFunction* f,
			const std::vector<InferenceData>& scopeStack, const multiname* name);
	static EARLY_BIND_STATUS earlyBindForScopeStack(std::ostream& out, const SyntheticFunction* f,
			const std::vector<InferenceData>& scopeStack, const multiname* name, InferenceData& inferredData);
	static const Type* getLocalType(const SyntheticFunction* f, unsigned localIndex);

	bool addEvent(_NR<EventDispatcher>,_R<Event> ) DLL_PUBLIC;
	bool prependEvent(_NR<EventDispatcher>,_R<Event> ) DLL_PUBLIC;
	int getEventQueueSize();
	void shutdown();
	bool hasEverStarted() const { return status!=CREATED; }

	static Global* getGlobalScope(call_context* th);
	static bool strictEqualImpl(ASObject*, ASObject*);
	static void publicHandleEvent(_R<EventDispatcher> dispatcher, _R<Event> event);
	static _R<ApplicationDomain> getCurrentApplicationDomain(call_context* th);
	static _R<SecurityDomain> getCurrentSecurityDomain(call_context* th);

	/* The current recursion level. Each call increases this by one,
	 * each return from a call decreases this. */
	uint32_t cur_recursion;
	std::vector<std::pair<uint32_t,ASObject*> > stacktrace;

	struct abc_limits {
		/* maxmium number of recursion allowed. See ScriptLimitsTag */
		uint32_t max_recursion;
		/* maxmium number of seconds for script execution. See ScriptLimitsTag */
		uint32_t script_timeout;
	} limits;

	uint32_t getAndIncreaseNamespaceBase(uint32_t nsNum);

	tiny_string getDefaultXMLNamespace();
	uint32_t getDefaultXMLNamespaceID();

	bool buildClassAndBindTag(const std::string& s, DictionaryTag* t);
	void checkExternalCallEvent() DLL_PUBLIC;
};

class DoABCTag: public ControlTag
{
private:
	ABCContext* context;
public:
	DoABCTag(RECORDHEADER h, std::istream& in);
	TAGTYPE getType() const{ return ABC_TAG; }
	void execute(RootMovieClip* root) const;
};

class DoABCDefineTag: public ControlTag
{
private:
	UI32_SWF Flags;
	STRING Name;
	ABCContext* context;
public:
	DoABCDefineTag(RECORDHEADER h, std::istream& in);
	TAGTYPE getType() const{ return ABC_TAG; }
	void execute(RootMovieClip* root) const;
};

class SymbolClassTag: public ControlTag
{
private:
	UI16_SWF NumSymbols;
	std::vector<UI16_SWF> Tags;
	std::vector<STRING> Names;
public:
	SymbolClassTag(RECORDHEADER h, std::istream& in);
	TAGTYPE getType() const{ return SYMBOL_CLASS_TAG; }
	void execute(RootMovieClip* root) const;
};

ASObject* undefinedFunction(ASObject* obj,ASObject* const* args, const unsigned int argslen);

inline ABCVm* getVm(SystemState* sys)
{
	return sys->currentVm;
}

std::istream& operator>>(std::istream& in, method_info& v);

};

#endif /* SCRIPTING_ABC_H */
