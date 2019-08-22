/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdint>
#include <cstddef>
#include <string>
#include <ostream>
#include <sstream>
#include <stack>
#include <set>
#include <vector>
#include <tuple>

#include <test/tools/ossfuzz/yulProto.pb.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>

namespace yul
{
namespace test
{
namespace yul_fuzzer
{
class ProtoConverter
{
public:
	ProtoConverter()
	{
		m_numLiveVars = 0;
		m_invisibleVarsInFunction = 0;
		m_visibleFunctionIndex = 0;
		m_numVarsPerScope.push(m_numLiveVars);
		m_numFunctionsNoRet = 0;
		m_numFunctionsSingleRet = 0;
		m_numFunctionsMultiRet = 0;
		m_inForBodyScope = false;
		m_inForInitScope = false;
		m_numNestedForLoops = 0;
		m_counter = 0;
		m_inputSize = 0;
		m_inFunctionDef = false;
	}
	ProtoConverter(ProtoConverter const&) = delete;
	ProtoConverter(ProtoConverter&&) = delete;
	std::string programToString(Program const& _input);

private:
	void visit(BinaryOp const&);
	void visit(Block const&);
	void visit(Literal const&);
	void visit(VarRef const&);
	void visit(Expression const&);
	void visit(VarDecl const&);
	void visit(TypedVarDecl const&);
	void visit(UnaryOp const&);
	void visit(AssignmentStatement const&);
	void visit(IfStmt const&);
	void visit(StoreFunc const&);
	void visit(Statement const&);
	void visit(ForStmt const&);
	void visit(BoundedForStmt const&);
	void visit(CaseStmt const&);
	void visit(SwitchStmt const&);
	void visit(TernaryOp const&);
	void visit(NullaryOp const&);
	void visit(LogFunc const&);
	void visit(CopyFunc const&);
	void visit(ExtCodeCopy const&);
	void visit(StopInvalidStmt const&);
	void visit(RetRevStmt const&);
	void visit(SelfDestructStmt const&);
	void visit(TerminatingStmt const&);
	void visit(FunctionCall const&);
	void visit(FunctionDef const&);
	void visit(Program const&);

	std::string createHex(std::string const& _hexBytes);
	std::string createAlphaNum(std::string const& _strBytes);
	bool isCaseLiteralUnique(Literal const&);
	enum class NumFunctionReturns
	{
		None,
		Single,
		Multiple
	};

	template<class T>
	void visitFunctionInputParams(T const&, unsigned);

	template<class T>
	void createFunctionDefAndCall(T const&, unsigned, unsigned, NumFunctionReturns);
	std::string functionTypeToString(NumFunctionReturns _type);
	/// Returns current index of function and increments it thereafter.
	std::string functionTypeToIndex(NumFunctionReturns _type);

	/// Return true if at least one variable declaration is in scope,
	/// false otherwise.
	/// @return True in the following cases:
	/// - If we are inside a function that has already declared a variable
	/// - If there is at least one variable declaration that is
	/// in scope
	bool varDeclAvailable();

	/// Return true if a function call cannot be made, false otherwise.
	/// @param _type is an enum denoting the type of function call. It
	/// can be one of NONE, SINGLE, MULTIDECL, MULTIASSIGN.
	/// 			NONE -> Function call does not return a value
	///				SINGLE -> Function call returns a single value
	///				MULTIDECL -> Function call returns more than one value
	///					and it is used to create a multi declaration
	///					statement
	///				MULTIASSIGN -> Function call returns more than one value
	///					and it is used to create a multi assignment
	///					statement
	/// @return True if the function call cannot be created for one of the
	/// following reasons
	//   - It is a SINGLE function call (we reserve SINGLE functions for
	//   expressions)
	//   - It is a MULTIASSIGN function call and we do not have any
	//   variables available for assignment.
	bool functionCallNotPossible(FunctionCall_Returns _type);

	/// Makes a call to the function of type `_type` and index `_funcIndex` containing
	/// `_numInputs` input and `_numOutputs` output parameters.
	void scopedFunctionCall(
		unsigned _numInputs,
		unsigned _numOutputs,
		std::string _funcIndex,
		NumFunctionReturns _type
	);

	template <class T>
	void registerFunction(T const& _x, NumFunctionReturns _type, unsigned _numOutputParams = 0);

	/// Returns a pseudo-random dictionary token.
	/// @param _p Enum that decides if the returned token is hex prefixed ("0x") or not
	/// @return Dictionary token at the index computed using a
	/// monotonically increasing counter as follows:
	///		index = (m_inputSize * m_inputSize + counter) % dictionarySize
	/// where m_inputSize is the size of the protobuf input and
	/// dictionarySize is the total number of entries in the dictionary.
	std::string dictionaryToken(dev::HexPrefix _p = dev::HexPrefix::Add);

	/// Returns a monotonically increasing counter that starts from zero.
	unsigned counter()
	{
		return m_counter++;
	}

	std::ostringstream m_output;
	// Number of live variables in inner scope of a function
	std::stack<unsigned> m_numVarsPerScope;
	// Number of live variables in function scope
	unsigned m_numLiveVars;
	/// Last variable that is in parent (ancestor) scope of function.
	unsigned m_invisibleVarsInFunction;
	/// Index of visible function
	unsigned m_visibleFunctionIndex;
	// Set that is used for deduplicating switch case literals
	std::stack<std::set<dev::u256>> m_switchLiteralSetPerScope;
	// Total number of functions.
	unsigned m_numFunctionsNoRet;
	unsigned m_numFunctionsSingleRet;
	unsigned m_numFunctionsMultiRet;
	// Look-up table per function type that holds the number of input (output) function parameters
	std::vector<unsigned> m_functionVecNoReturnValue;
	std::vector<unsigned> m_functionVecSingleReturnValue;
	std::vector<std::pair<unsigned, unsigned>> m_functionVecMultiReturnValue;
	// mod input/output parameters impose an upper bound on the number of input/output parameters a function may have.
	static unsigned constexpr modInputParams = 5;
	static unsigned constexpr modOutputParams = 5;
	// predicate to keep track of for body scope
	bool m_inForBodyScope;
	// Index used for naming loop variable of bounded for loops
	unsigned m_numNestedForLoops;
	// predicate to keep track of for loop init scope
	bool m_inForInitScope;
	/// Monotonically increasing counter
	unsigned m_counter;
	/// Size of protobuf input
	unsigned m_inputSize;
	/// predicate to keep track of function scope
	bool m_inFunctionDef;
};
}
}
}
