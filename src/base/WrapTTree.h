#pragma once

#include "TTree.h"

#include <vector>
#include <string>
#include <algorithm>

namespace ant {

/**
 * @brief The WrapTTree struct simplifies TTree handling in physics classes
 *
 * Example usage (see also etaprime_omega_gamma.h):
 *
 * struct treeTest_t : WrapTTree {
 *   ADD_BRANCH_T(bool,   IsSignal)
 *   ADD_BRANCH_T(double, KinFitChi2)
 *   // ... more branches using macro ADD_BRANCH_T
 * };
 *
 * treeTest_t treeTest;
 *
 * ... in constructor, setup for "writing" ...
 *
 *    treeTest.CreateBranches(HistFac.makeTTree("test"));
 *
 * ... get/set branches via operator() ...
 *
 *    treeTest.IsSignal() = b_IsSignal;
 *
 * ... fill (or get entry) ...
 *
 *   treeTest.Tree->Fill();
 *
 */
class WrapTTree {
public:
    /**
     * @brief Tree to be used as usual TTree
     */
    TTree* Tree = nullptr;

    /**
     * @brief CreateBranches prepares the instance for filling the TTree
     * @param tree the tree to be filled
     */
    void CreateBranches(TTree* tree);

    /**
     * @brief LinkBranches prepares the instance for reading the TTree
     * @param tree the tree to read from, or use already set Tree class member
     */
    void LinkBranches(TTree* tree = nullptr);

    /**
     * @brief Matches checks if the branch names are all available
     * @param tree the tree to check
     * @param exact if false, the TTree may have additional branches
     * @return true if successful
     */
    bool Matches(TTree* tree = nullptr, bool exact = true, bool nowarn = false) const;

    /**
     * @brief CopyFrom copies contents in branches by name
     * @param src the source of the contents to be copied
     * @return true if successful, false on mismatch
     */
    bool CopyFrom(const WrapTTree& src);

    /**
     * @brief operator bool returns true if Tree is not null
     */
    explicit operator bool() const {
        return Tree != nullptr;
    }

    template<typename T>
    struct Branch_t {
        template<typename... Args>
        Branch_t(WrapTTree& wraptree, const std::string& name, Args&&... args) :
            Name(name),
            // can't use unique_ptr because of std::addressof below
            Value(new T(std::forward<Args>(args)...))
        {
            static_assert(std::is_same<T, TClonesArray>::value ? sizeof... (Args) > 0 : true,
                          "TClonesArray cannot be default constructed (provide contained class as string!)");
            if(Name.empty())
                throw Exception("Branch name empty");
            auto b = ROOT_branch_t::Make(Name, std::addressof(Value.Ptr));
            auto& branches = wraptree.branches;
            // check if branch with this name already exists
            if(std::find(branches.begin(), branches.end(), b) != branches.end())
                throw Exception("Branch with name '"+Name+"' already exists");
            branches.emplace_back(std::move(b));
        }
        ~Branch_t() = default;
        Branch_t(const Branch_t&) = delete;
        Branch_t& operator= (const Branch_t& other) {
            *Value = *(other.Value);
            return *this;
        }
        Branch_t(Branch_t&&) = delete;
        Branch_t& operator= (Branch_t&&) = delete;

        const std::string Name;

        // implicit conversion
        operator T& () { return *Value; }
        operator const T& () const { return *Value; }
        // assignment
        T& operator= (const T& v) { *Value = v; return *Value; }
        T& operator= (T&& v) { *Value = v; return *Value; }
        // if you need to call methods of T, sometimes operator() is handy
        T& operator() () { return *Value; }
        const T& operator() () const { return *Value; }
    protected:
        struct Value_t {
            explicit Value_t(T* ptr) : Ptr(ptr) {}
            T& operator* () { return *Ptr; }
            const T& operator* () const { return *Ptr; }
            ~Value_t() {
                delete Ptr;
            }
            // forbid copy/move completely
            Value_t(const Value_t&) = delete;
            Value_t& operator=(const Value_t&) = delete;
            Value_t(Value_t&&);
            Value_t& operator=(Value_t&&) = delete;
            // access by Branch_t ctor
            T* Ptr;
        };
        Value_t Value;
    };

    struct Exception : std::runtime_error {
        // use ctors
        using std::runtime_error::runtime_error;
    };

protected:
    // force user to inherit from this class
    // use ADD_BRANCH_T to define branches (see comments above as well)
    WrapTTree() = default;

    struct ROOT_branchinfo_t {
        std::string Name;
        TClass*   ROOTClass = nullptr;
        EDataType ROOTType  = kOther_t;
        explicit ROOT_branchinfo_t(const std::string& name) : Name(name) {}
        bool operator==(const ROOT_branchinfo_t& other) {
            return Name == other.Name;
        }
    };

    struct ROOT_branch_t : ROOT_branchinfo_t {
        // need templated factory as ROOT_branch_t is not templated
        template<typename T>
        static ROOT_branch_t Make(const std::string& name, T** valuePtr) {
            ROOT_branch_t b(name);
            b.ValuePtr = (void**)valuePtr;
            // the following is copied from TTree::SetBranchAddress<T>
            b.ROOTClass = TClass::GetClass(typeid(T));
            b.ROOTType = kOther_t;
            if (b.ROOTClass==0) b.ROOTType = TDataType::GetType(typeid(T));
            return b;
        }
        // cannot be const as WrapTTree should stay copy-assignable
        void** ValuePtr;
    protected:
        // use ctor from base class
        using ROOT_branchinfo_t::ROOT_branchinfo_t;
    };

    std::vector<ROOT_branch_t> branches;
};

}

// macro to define branches consistently
#define ADD_BRANCH_T(type, name, args...) Branch_t<type> name{*this, #name, args};
