#pragma once

#include <string>
#include <cstring>


namespace Util {


//mimic the same thing as boost
template <typename T>
void hash_combine(std::size_t& seed, const T& v)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}


// ================================================================
// Basic hash and compare functor types
// ================================================================

template <typename T> struct Hash : public std::hash<T> {};

template <typename T> struct Comp {
    bool  operator() (const T& a, const T& b) const { return a == b; }
};

template <> struct Hash< const char* > {
    size_t  operator() (const char* s) const
                        {
                            std::size_t  seed = 0;
                            for ( ; *s; s++)
                                hash_combine( seed, *s );
                            return seed;
                        }
};

template <> struct Comp< const char* > {
    bool  operator() (const char* a, const char* b) const { return strcmp(a, b) == 0; }
};

// can be used for both Hasher and Comper
struct BinStringNoCase {
    // Compare
    bool  operator() (const char* a, const char* b) const { return strcasecmp(a, b) == 0; }
    bool  operator() (const std::string& a, const std::string& b) const
                        { return strcasecmp(a.c_str(), b.c_str()) == 0; }
    // Hash
    size_t  operator() (const char* s) const
                        {
                            std::size_t  seed = 0;
                            for ( ; *s; s++)
                                hash_combine( seed, tolower(*s) );
                            return seed;
                        }
    size_t  operator() (const std::string& s) const
                        {   return (*this)( s.c_str() );  }
};


// can be used for both Hasher and Comper
class BinStringCaseSelectable {
    bool   case_sense;
    int    (*cmp_func) (const char* s1, const char* s2);
public:
    BinStringCaseSelectable (bool case_sense_)
                        : case_sense( case_sense_ ),
                          cmp_func ( case_sense_ ? strcmp : strcasecmp )
                        {}
    // Compare
    bool  operator() (const char* a, const char* b) const
                        { return ( (*cmp_func)(a, b) == 0 ); }
    bool  operator() (const std::string& a, const std::string& b) const
                        { return ( (*cmp_func)(a.c_str(), b.c_str()) == 0); }
    // Hash
    std::size_t  operator() (const char* s) const
                        {
                            std::size_t  seed = 0;
                            if (case_sense)
                                for ( ; *s; s++) hash_combine( seed, *s );
                            else
                                for ( ; *s; s++) hash_combine( seed, tolower(*s) );
                            return seed;
                        }
    size_t  operator() (const std::string& s) const
                        {   return (*this)( s.c_str() );  }
};


// data type specific functions for hash and compare
template <typename T,
          typename HashT = Util::Hash<T>, // requires: size_t operator() (const T&) const;
          typename CompT = Util::Comp<T>  // requires: bool operator() (const T&, const T&) const;
          >
class SearchOps
{
    HashT  d_hash;
    CompT  d_compare;
public:
    SearchOps ()                                  {}
    SearchOps (const HashT& h)                    : d_hash(h) {}
    SearchOps (const HashT& h, const CompT& c)    : d_hash(h), d_compare(c) {}
    size_t  operator() (const T& x)               { return d_hash(x); }
    bool    operator() (const T& a, const T& b)   { return d_compare(a, b); }
};


} // namesapce util

#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <cstring>
#include <ctype.h>
#include <cassert>

#include "HashOps.h"

namespace Util {


// ================================================================
// hash base
//
// do the hash calculations and control the vectors, but don't own
// any memory.
//
// TODO: make this the base of HashSet and HashMap in "Hash.h"
// ================================================================

// requires:
//    T = Entries::value_type;
//
//    IdType   T::next () const;
//    void     T::next (IdType id);
//
//    void  T::operator= (AccessT val);  // or ref or const ref
//    operator AccessT () const;

// //    // ONLY needed if 'remove' is called           possible implementations:
// //    bool  T::removed(IdType);  // is it removed.   bool  removed (int i) 
// //    void  T::remove(IdType);   // tag as removed. needed if 'remove' is called
// 
//    IdType = IndexAryT::value_type
// 
//    size_t  Ops::operator() (const T& x);  // hash value of 't'
//    bool    Ops::operator() (const T& a, const T& b);  // a == b

template <typename Entries,     // indexable AuData type (StringArray, Array, VarArray, ...)
                                //  or std::vector<T>, std::deque<T>
          typename Indexes,     // hash table array type (e.g., Array<int>, std::vector<size_t>)
          typename Ops = SearchOps< typename Entries::value_type >,
          typename AccessT = typename Entries::value_type
          >
class HashBase
{
    using  T  = typename Entries::value_type;
    using  Id = typename Indexes::value_type;
    static constexpr  Id  EOL = ~0;

    // // enable 'remove' function by adding "using  Removable = bool;" to T class. TODO: FIX
    // //// enable 'remove' function by adding "constexpr bool  Removable = true;" to T class.

    // // primary template handles T witout nested ::Removable member
    // template <typename, typename = void>
    // struct  Removable : std::false_type {};
 
    // // specialization recognizes T with nested ::Removable member
    // template <>
    // struct  Removable< T, std::void_t<typename T::Removable>> : std::true_type {};

    // ==== Id's for 'remove' -- if remove is not used, -1 is the only special index
    //        Id >= 0:   index in 'd_entries' of next hash bin entry.
    //        Id == -1:  end of hash bin list.
    //        Id == -2:  end of free_list.
    //        Id < -2:   index (= -3 - 'next')  in 'd_entries' of next free
    static inline Id  _free_list_link (Id x)  { return -3 - x; }

public:

    // grouped data fields needed for serialization
    class Attrs
    {
        friend class HashBase;

        Id            free_list   = -2;     // index of first unused d_entries slot; others are linked
        Id            size        = 0;      // number of non-removed entries (not d_entries.size())

        // user settable at construction only
        size_t        hash_size   = 101;    // number of hash buckets (size of d_table)
        bool          pre_filled  = false;  // set true if contructor input vec's contain valid data

        // user settable at any time
        int           max_depth   = 4;      // rehash occurs if average depth exceeds this value
        int           rehash_mult = 4;      // if a rehash is needed, by how much is hash_size increased

    public:
        Attrs () {}
        // for chaining
        Attrs&  set_hash_size (int x)           { hash_size = x;     return *this; }
        Attrs&  set_max_depth (int x)           { max_depth = x;     return *this; }
        Attrs&  set_rehash_mult (int x)         { rehash_mult = x;   return *this; }
        Attrs&  set_pre_filled (bool x = true)  { pre_filled = x;    return *this; }
    };

private:

    Attrs     d;

    T           d_null_val;  // removed items are set to this, so real entries can be deleted

    Indexes&    d_table;
    Entries&    d_entries;   // entries
    Ops&        d_ops;       // functors to compute hash value from T and to compare two T's


public:

    // samples:
    //    // create new with hash_size = 42
    //    HashBase  h( evec, hvec, ops, Attrs().set_hash_size(42) );
    //
    //    // create from previous vectors and set max_depth = 10
    //    HashX::Attrs ...
    //    HashBase  h( evec, hvec, ops, Attrs().set_pre_filled().set_max_depth(10) );

    HashBase (Entries& entries_vec, // memory for values and 'next' index
              Indexes& table_vec,   // memory for hash table
              Ops& search_ops,
              const Attrs& attrs = Attrs());

    // to set search hash parameters during construction
    void  set_null_val (const T& x)             { d_null_val = x; }

    void  predict (size_t nnodes);

    Attrs&  attrs ()                            { return d; }

    // ==== insertions / deletions
    // Insert 'v' if table does not already contain an entry matching 'v'.
    // If inserting (no previous match to v), return pair( 'v', true ),
    // else return pair( previous match to 'v', false ).
    std::pair<const AccessT&, bool>  insert (const AccessT& v);

    // Adds 'v' into the table regardless of whether the table already contain an entry matching 'v'.
    // If inserting (no previous match to v), return pair( 'v', true ),
    // else if assigning (previous match to v), return pair( previous match to 'v', false ).
    std::pair<const AccessT&, bool>  insert_or_assign (const AccessT& v);

    void  rehash (size_t new_hash_size);

    // Return true if an entry matching 'v' existed and was removed.
    bool  remove (const AccessT& v);

    void  remove_by_index (Id i);

    // remove all entries. does not change the hash_size.
    void  clear ();

    // ==== access
    bool  empty () const      {  return d.size == 0;  }
    size_t  size () const     {  return d.size;  }
    bool  contains (const AccessT& v) const;

    // if set contains 'v', return pair( ref to set's value, true ), else return pair('v', false)
    std::pair<const AccessT&, bool>  find (const AccessT& v) const;

    // ================ direct indexing support
    // same as above, but return the entry id as the first element of the return pair
    std::pair<Id, bool>  insert2 (const AccessT& v);
    std::pair<Id, bool>  insert_or_assign2 (const AccessT& v);

    // Appends 'v' into the table regardless of whether the table already contain an entry matching 'v'.
    // If 'v' already exists, only one is returned with 'find' (likely the new entry).
    Id  insert_end (const AccessT& v);

    // return ~0 if not found
    Id  find2 (const AccessT& v) const;

    // pair.second is false for a bad index or deleted element
    std::pair<const AccessT&, bool>  at (Id id) const;

    // ==== iterate ====
    // usage:
    //   for (HashSet<T,...>::Iterator it(hash_set);  it();  ++it) {
    //       const T&  t = *it;
    //       ...
    //   }
    // or
    //   for (const T& val : hash_base) { ... }
    class Iterator {
        using  Iter = typename Entries::iterator;
        Iter  it, end;
        void  _advance ()      { while (it != end && it->next() < EOL) ++it; } // skip deletions
        Iterator (Iter i, Iter e)       : it(i), end(e) { _advance(); }
    public:
        Iterator (HashBase& h)          : it( h.d_entries.begin() ), end( h.d_entries.end() ) { _advance(); }
        bool  operator() ()             {  return it != end;  }
        void  operator++ ()             {  if (it != end) ++it;  _advance();  } // skip deletions
        const AccessT&  operator* ()    {  return it->val();  }
        bool  operator!= (Iterator& x)  { return it != end; }
    };

    Iterator  begin ()                  { return Iterator( d_entries.begin(), d_entries.end() ); }
    Iterator  end ()                    { return Iterator( d_entries.end(),   d_entries.end() ); }

    // ================
    // ==== stats

    size_t  bucket_count () const       {  return d.hash_size;  }
    void  print_histogram (FILE* f = NULL) const; // NULL = stdout

private:

    Id  _new_entry (const T& val, Id next)
                        {
                            d.size++;
                            if (/* Removable and */ d.free_list != _free_list_link(EOL)) {
                                Id  idx = _free_list_link( d.free_list );
                                d.free_list = d_entries[idx].next();
                                d_entries[idx] = val;
                                d_entries[idx].next( next );
                                return idx;
                            }
                            else {
                                Id  idx = d_entries.size();
                                d_entries.emplace_back( val );
                                d_entries[idx].next( next );
                                return idx;
                            }
                        }
};


// ================================================================
//  Details:
//   methods for hash set
// ================================================================

// ==== create new
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
HashBase<Entries,Indexes,Ops,AccessT>::HashBase (Entries& entries,
                                                 Indexes& table,
                                                 Ops& search_ops,
                                                 const Attrs& attrs_)
    : d( attrs_ ),
      d_table( table ),
      d_entries( entries ),
      d_ops( search_ops )
{
    if (not d.pre_filled) {
        d_table.resize( d.hash_size, EOL );
        for (size_t i = 0;  i < d.hash_size;  i++)
            d_table[i] = EOL;
        d.pre_filled = true;
    }
    else
        assert( d.hash_size == table.size() );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
void  HashBase<Entries,Indexes,Ops,AccessT>::predict (size_t nnodes)
{
    rehash( nnodes * 21 / 20 / d.max_depth );  // add 5%
    d_entries.reserve(nnodes);
}


// Insert 'v' if set does not already contain an entry matching 'v'.
// If inserting (no previous match to v), return pair( 'v', true ),
// else return pair( previous match to 'v', false ).
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
inline std::pair< const AccessT&, bool >  HashBase<Entries,Indexes,Ops,AccessT>::insert (const AccessT& v)
{
    std::pair<Id, bool>  x = insert2( v );
    return std::pair<const AccessT&, bool>( d_entries[x.first].val(), x.second );
}


// Adds 'v' into the set regardless of whether the set already contain an entry matching 'v'.
// If inserting (no previous match to v), return pair( 'v', true ),
// else if assigning (previous match to v), return pair( previous match to 'v', false ).
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
inline std::pair< const AccessT&, bool >
HashBase<Entries,Indexes,Ops,AccessT>::insert_or_assign (const AccessT& v)
{
    std::pair<Id, bool>  x = insert_or_assign2( v );
    return std::pair<const AccessT&, bool>( d_entries[x.first].val(), x.second );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
void  HashBase<Entries,Indexes,Ops,AccessT>::rehash (size_t new_hash_size)
{
    // clear new hash table
    d_table.resize(new_hash_size, EOL);
    for (Id i = 0;  i < Id(new_hash_size);  i++)
        d_table[i] = EOL;
    // go through entries, compute new table slots and build new list links
    for (Id i = 0, j = d_entries.size();  i < j;  i++) {
        if (d_entries[i].next() >= EOL) {
            size_t  h = d_ops( d_entries[i] ) % new_hash_size;
            d_entries[i].next( d_table[h] );
            d_table[h] = i;
        }
    }
    d.hash_size = new_hash_size;
}

// Return true if an entry matching 'v' existed and was removed.
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
bool  HashBase<Entries,Indexes,Ops,AccessT>::remove (const AccessT& v)
{
    size_t  h = d_ops( v ) % d.hash_size;
    for (Id* i = d_table.data() + h;  *i != EOL;  i = &(d_entries[*i].next())) {
        T&  e = d_entries[*i];
        if (d_ops( v, e )) {
            Id  idx = *i;
            *i = e.next();
            e = d.null_val; // force destruction of removed entry, if relevant
            e.next( d.free_list );
            d.free_list = _free_list_link( idx );
            d.size--;
            return true;
        }
    }
    return false;
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
void  HashBase<Entries,Indexes,Ops,AccessT>::clear ()
{
    for (Id i = 0;  i < d.hash_size;  i++)
        d_table[i] = EOL;
    d_entries.clear();
    d.size = 0;
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
bool  HashBase<Entries,Indexes,Ops,AccessT>::contains (const AccessT& v) const
{
    size_t  h = d_ops( v ) % d.hash_size;
    for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next()) {
        if (d_ops( v, d_entries[i] ))
            return true;
    }
    return false;
}


// if set contains 'v', return pair( ref to set's value, true ), else return pair('v', false)
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
std::pair< const AccessT&, bool >
HashBase<Entries,Indexes,Ops,AccessT>::find (const AccessT& v) const
{
    size_t  h = d_ops( v ) % d.hash_size;
    for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next()) {
        if (d_ops( v, d_entries[i] ))
            return std::pair<const T&, bool>( d_entries[i].val(), true );
    }
    return std::pair<const AccessT&, bool>( v, false );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
std::pair<typename Indexes::value_type, bool>  HashBase<Entries,Indexes,Ops,AccessT>::insert2 (const AccessT& v)
{
    size_t  h = d_ops( v ) % d.hash_size;
    for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next()) {
        if (d_ops( v, d_entries[i] ))
            return std::pair<Id, bool>( i, false );
    }
    if (d.size / d.hash_size > size_t(d.max_depth)) {
        this->rehash( d.rehash_mult * d.hash_size );
        h = d_ops( v ) % d.hash_size;
    }
    Id  idx = this->_new_entry( v, d_table[h] );
    d_table[h] = idx;
    return std::pair<Id, bool>( idx, true );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
std::pair< typename Indexes::value_type, bool >
HashBase<Entries,Indexes,Ops,AccessT>::insert_or_assign2 (const AccessT& v)
{
    size_t  h = d_ops( v ) % d.hash_size;
    for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next()) {
        if (d_ops( v, d_entries[i] )) {
            d_entries[i].val() = v;
            return std::pair<ssize_t, bool>( i, false );
        }
    }
    if (d.size / d.hash_size > d.max_depth) {
        this->rehash( d.rehash_mult * d.hash_size );
        h = d_ops( v ) % d.hash_size;
    }
    Id  idx = this->_new_entry( v, d_table[h] );
    d_table[h] = idx;
    return std::pair<ssize_t, bool>( idx, true );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
typename Indexes::value_type  HashBase<Entries,Indexes,Ops,AccessT>::insert_end (const AccessT& v)
{
    if (d.size / d.hash_size > size_t(d.max_depth))
        this->rehash( d.rehash_mult * d.hash_size );

    Id  idx = d_entries.size();
    d_entries.emplace_back( v );
    size_t  h = d_ops( v ) % d.hash_size;
    d_entries[idx].next( d_table[h] );
    d_table[h] = idx;
    d.size++;
    return idx;
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
typename Indexes::value_type  HashBase<Entries,Indexes,Ops,AccessT>::find2 (const AccessT& v) const
{
    size_t  h = d_ops( v ) % d.hash_size;

    for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next()) {
        if (d_ops( v, d_entries[i] ))
            return i;
    }
    return -1;
}


// pair.second is false for a bad index or deleted element
template <typename Entries, typename Indexes, typename Ops, typename AccessT>
std::pair<const AccessT&, bool>  HashBase<Entries,Indexes,Ops,AccessT>::at (Id id) const
{
    if (size_t(id) < d_entries.size()) {
        if (d_entries[id].next() >= -1)
            return std::pair<const T&, bool>( d_entries[id].val(), true );
    }
    return std::pair<const T&, bool>( d.null_val, false );
}


template <typename Entries, typename Indexes, typename Ops, typename AccessT>
void  HashBase<Entries,Indexes,Ops,AccessT>::print_histogram (FILE* f) const
{
    if (!f) f = stdout;
    std::map< Id, int >  hist;
    for (size_t h = 0;  h < d.hash_size;  h++) {
        int  sz = 0;
        for (Id i = d_table[h];  i != EOL;  i = d_entries[i].next())
            sz++;
        hist[sz] += 1;
    }

    fprintf( f, "Table Histogram:\n" );
    for (auto x : hist)
        fprintf( f, "   %5lu %8d\n", x.first, x.second );

    fprintf( f, "\n" );
}


} // namesapce Util


template <typename T>
class SparseArray : public DataObj
{
    T                                      dflt   = T{};
    size_t                                 d_size = 0;
    size_t                                 offset = 0;
    size_t                                 siz    = ~0;
    std::shared_ptr<Util::HashMap<int, T>> d;

    SparseArray (SchemaPtr sch_,std::shared_ptr<Util::HashMap<int, T>> d_, size_t offset_, size_t slice_size_, T dflt_)
        :DataObj(sch_), dflt(dflt_), offset(offset_), siz(slice_size_), d(d_) {}
public:
    class Ref
    {
    private:
        using HashMap = Util::HashMap<int, T>;
    public:
        T _dflt;
        int _n;
        HashMap& _d;

        Ref(T dflt_, int n_, HashMap& d_) : _dflt(dflt_), _n(n_), _d(d_) {};

        Ref& operator=(T value);

        friend std::ostream&  operator<< (std::ostream& s, const Ref& c)
        {
            auto val = [iter = c._d.find(c._n), &c](){ if(iter.second) return iter.first; else return c._dflt; };
            return s<< val();
        }

    };

public:
    SparseArray (SchemaPtr sch)
        : DataObj(sch), d(std::make_shared<Util::HashMap<int, T>>()) {}

    SparseArray (SchemaPtr sch, size_t sz)
        : DataObj(sch), d_size( sz ), d(std::make_shared<Util::HashMap<int, T>>()) {}

    SparseArray (SchemaPtr sch, size_t sz, const T& dflt_)
        : DataObj(sch), dflt(dflt_), d_size(sz),d(std::make_shared<Util::HashMap<int, T>>()) {}

    size_t          size () const override              { return std::min(d_size - offset, siz); }

    size_t          size_bytes () const override        { return 0; } // TODO:  sizeof(T) * d.size(); }

    DataObjPtr      slice (size_t offset, size_t slice_size = ~0) override;

    DataObjPtr      slice_copy ( size_t offset, size_t slice_size = ~0) override;

    Ref       operator[] ( Id<T> i )                    { return Ref(dflt, i.get() + offset, *d); }

    const Ref operator[] ( Id<T> i ) const              { return Ref(dflt, i.get() + offset, *d); }

    const void*     vget (int i) const override;
    GenericValue    gvget (int i) const override;

    void            vset (int i, const void* val) override;

    void            vappend (const void* val) override;

    void            serialize (MPSys::Message& msg, DataObjPtr p) override;
    void            unload () override;

    void            repr (std::ostream& s, int level) const override;
    void            print (int level=0) const override;
};

template<typename T>
DataObjPtr  SparseArray<T>::slice (size_t offset, size_t slice_size)
{
    SparseArrayData<T>* sliced = new SparseArrayData<T>(this->schema(), d, offset, slice_size, dflt);
    sliced->d_parent = d_parent;
    return DataObjPtr(sliced);
}

template<typename T>
DataObjPtr SparseArray<T>::slice_copy ( size_t offset, size_t slice_size)
{
    SchemaPtr sch = SchemaPtr( this->schema()->copy() );
    std::shared_ptr<Util::HashMap<int, T>> op = std::make_shared< Util::HashMap<int, T> >( *d );
    SparseArray<T>* copy_sparse_array_data = new SparseArray<T>(sch, op, offset, slice_size, dflt);
    return DataObjPtr( copy_sparse_array_data );
}

template<typename T>
typename SparseArray<T>::Ref& SparseArray<T>::Ref::operator=(T value)
{
    if (value != _dflt)
        _d[_n] = value;
    return *this;
}

template<typename T>
const void*  SparseArray<T>::vget (int i) const
{
    auto non_const_this = const_cast<SparseArray<T>*> (this);

    std::pair<T&, bool>  iter = non_const_this->d->find(i+offset);
    if (iter.second)
        return (const void*) &(iter.first); // TODO: address of local variable???
    else
        return (const void*) &dflt;
}

template<typename T>
GenericValue  SparseArray<T>::gvget (int i) const
{
    auto non_const_this = const_cast<SparseArray<T>*> (this);

    std::pair<T&, bool>  iter = non_const_this->d->find(i+offset);
    if (iter.second)
        return GenericValue( iter.first );
    else
        return GenericValue( dflt );
}


template<typename T>
void  SparseArray<T>::vset (int i, const void* val)
{
    const T&  v = * (const T*) val;
    if (v != dflt)
        (*d)[i+offset] = v;
    else
        d->remove(i);           // ok if it doesn't exist in 'd'
}

template<typename T>
void  SparseArray<T>::vappend (const void* val)
{
    assert( siz == (size_t)~0);
    const T&  v = * (const T*) val;
    if (v != dflt)
        (*d)[d_size] = v;
    d_size++;
}

template<typename T>
void  SparseArray<T>::serialize (MPSys::Message& msg, DataObjPtr p)
{
    msg.frame( MPSys::make_frame< ValueReferenceData<size_t> >( offset, Defer(p) ));
    msg.frame( MPSys::make_frame< ValueReferenceData<size_t> >( d_size, Defer(p) ));
    msg.frame( MPSys::make_frame< ValueReferenceData<size_t> >( siz, Defer(p) ));
    msg.frame( MPSys::make_frame< StaticMemoryData >( d->get_hash_info(), Defer(p) ));
    //msg.frame( MPSys::make_frame< StaticMemoryData >( d->get_hash_table(), d->get_hash_info()->hash_size) );
    msg.frame( MPSys::make_frame< StdVectorReferenceData<int> >( d->get_hash_table(), Defer(p) ));
    using  HashMapData = typename Util::HashMap<int, T>::Base::Entry;
    msg.frame( MPSys::make_frame< StdVectorReferenceData<HashMapData> >( d->get_hash_data(), Defer(p) ));
}


template<typename T>
void  SparseArray<T>::unload ()
{
    offset = d_size = siz = 0L;
    d = std::make_shared< Util::HashMap<int, T> >();
    d_deferral->unload( *this );
}


#pragma once

#include "Hash.h"


namespace AuData {

// ================================================================

template <typename T>
class Dict : public DataObj {
public:

    using Id = int;
    using StringId = int;

    // HashSet entry type
    using V = std::pair<StringId, T>;  // [ index of string in the 'names' vector, data value ]

    // ================
    // Hash and compare function class for the hash table
    class NameIdPair {
        static const Id   NotSet = ~0; // use local rather than stored
        // Reason to use reference here: to make hasher and comper have the same things with nip;
        const char*&              local_name_;
        const std::vector<char>*  stored_names_;

    public:
        NameIdPair ( const char*& local_name, std::shared_ptr<std::vector<char>>& names )
                                                         : local_name_( local_name ),
                                                           stored_names_( names.get() ) {}

        Id           local_name( const char* name )      { local_name_ = name; return NotSet; }
        const char*  name ( StringId id ) const          { return id == NotSet ? local_name_ : stored_names_->data() + id; }

        void         operator= ( const NameIdPair& nip ) { local_name_ = nip.local_name_;
                                                           stored_names_ = nip.stored_names_; }
        // Hasher
        size_t       operator() ( V v ) const            { return Util::Hash<const char*>()( name( v.first ) ); }

        // Comper
        bool         operator() ( V a, V b ) const       { return strcmp( name( a.first ), name( b.first ) ) == 0; }
    };


    using Hset = Util::HashSet<V, NameIdPair, NameIdPair>;
    using Iter = typename std::vector<typename Hset::Entry>::iterator;

    // ================
    class Reference {
    public:
        std::string                        _key;
        std::shared_ptr<std::vector<char>> _names;
        std::shared_ptr<NameIdPair>        _nip;
        std::shared_ptr<Hset>              _d;

        Reference( const std::string& key_,
                   std::shared_ptr<std::vector<char>>& names_,
                   std::shared_ptr<NameIdPair>& nip_,
                   std::shared_ptr<Hset>& d_ )
            : _key( key_ ),  _names( names_ ), _nip( nip_ ), _d( d_ ) {};

        Reference& operator= ( T value );

        // unlike std::map or ordered_map, do NOT insert if key is not already in the dict, throw Error instead
        operator T () const;

        // friend std::ostream&  operator<< ( std::ostream& s, const Reference& c );
    };

    // ================
    class iterator {
        Iter _current;
        Dict& _d;
        using V = std::pair<std::string, Reference>;
    public:

        iterator( Iter iter_, Dict& d_ ) : _current( iter_ ), _d( d_ ) {}

        StringId       string_id () const               { return _current->first; }

        V              operator*()                      { std::string name = _d.nip->name( ( *_current ).val.first ); return V( name, _d[name] ); }
        const V        operator*() const                { std::string name = _d.nip->name( ( *_current ).val.first ); return V( name, _d[name] ); }
        iterator&      operator++() noexcept            { _current++; return *this; }
        iterator       operator++( int ) noexcept       { iterator temp( *this );  ++( *this ); return temp; }
        iterator       operator+( int i ) noexcept      { iterator temp( *this );  _current += i; return temp; }
        const iterator operator++( int ) const noexcept { iterator temp( *this );  ++( *const_cast<iterator*>( this ) ); return temp; }
        iterator&      operator--() noexcept            {  _current--; return *this; }
        iterator       operator--( int )  noexcept      { iterator temp( *this );  --( *this ); return temp; }
        iterator       operator-( int i )  noexcept     {  _current -= i; return *this; }
        const iterator operator--( int ) const noexcept { iterator temp( *this );  --( *const_cast<iterator*>( this ) ); return temp; }

        // comparison
        bool operator== ( const iterator& rhs ) const noexcept  { return _current == rhs._current; }
        bool operator!= ( const iterator& rhs ) const noexcept  { return _current != rhs._current; }
        bool operator<  ( const iterator& rhs ) const noexcept  { return _current < rhs._current; }
        bool operator>  ( const iterator& rhs ) const noexcept  { return _current > rhs._current; }
        bool operator<= ( const iterator& rhs ) const noexcept  { return _current <= rhs._current; }
        bool operator>= ( const iterator& rhs ) const noexcept  { return _current >= rhs._current; }
    };


    //public:      // why public?
    const char*                        dflt_name = nullptr; // what is this for?
    std::shared_ptr<std::vector<char>> names;
    std::shared_ptr<NameIdPair>        nip;
    std::shared_ptr<Hset>              d;

    Dict ( SchemaPtr sch_,
               std::shared_ptr<std::vector<char>>& names_,
               std::shared_ptr<NameIdPair>& nip_,
               std::shared_ptr<Hset>& d_ );

public: // ================================================================

    using value_type = T;

    Dict ( SchemaPtr sch );
    Dict ( SchemaPtr sch, const std::string& path );

    // If not find key in d, insert the value
    // If dind key in d, update the old value with provided one
    // returns string_id
    StringId        insert ( const std::string& key, T value );

    bool            contains ( const std::string& key ) const;

    // return value connected with 'key' if it exists in dictionary, else return 'dflt'
    const T&        lookup ( const std::string& key, const T& dflt = T{} ) const;
    // return id which can be efficently stored and used to lookup string with dict[str_id]; or ~0 if not found
    IdType          string_id ( const std::string& key ) const;

    // Operator= of reference will perform the same thing as insert function
    Reference       operator[] ( const std::string& key )    { return Reference( key, names, nip, d ); }

    const T&        operator[] ( const std::string& key ) const; // same as lookup, but throws excetion on error
    const T&        operator[] ( IdType id ) const;
    std::string     key_at ( IdType id ) const;

    bool            empty () const                           { return d->size() == 0; }

    void            clear ()                                 { std::vector<char>().swap( *names ); d->clear(); }

    iterator        find ( const std::string& key );

    iterator        begin ()                                 { return iterator( d->get_hash_data().begin(), *this );  }
    iterator        end ()                                   { return iterator( d->get_hash_data().end(), *this ); }

    const iterator  begin () const                           { return iterator( d->get_hash_data().begin(), *this );  }
    const iterator  end () const                             { return iterator( d->get_hash_data().end(), *this ); }

    size_t          size () const override                   { return d->size(); }
    size_t          size_bytes () const override             { return 0; }// TODO

    DataObjPtr      slice ( size_t offset, size_t slice_size = ~0 ) override;
    DataObjPtr      slice_copy ( size_t offset, size_t slice_size = ~0 ) override;

    void            repr (std::ostream& s, int level) const override;
    void            print ( int level = 0 ) const override;

    // ================ internal ================
    GenericValue    gvget ( const char* key ) const override;
    GenericValue    gvget ( int id ) const override;
    int             gvset ( const char* key, GenericValue val ) override;
    void            serialize ( MPSys::Message& msg, DataObjPtr p ) override;
    void            unload () override;
    void            set_reference_ptrs ( DataObjPtr this_node );
    DataObjPtr      reference ()        { return d_reference; }

private:
    std::string  d_path;
    DataObjPtr   d_reference;
};



} // namespace AuData

#pragma once


// ================================================================
// DETAILS
// ================================================================

namespace AuData {


template <typename T>
Dict<T>::Dict ( SchemaPtr sch_,
                std::shared_ptr<std::vector<char>>& names_,
                std::shared_ptr<NameIdPair>& nip_,
                std::shared_ptr<Hset>& d_ )
    : DataObj( sch_ ),
      names( names_ ),
      nip( nip_ ),
      d( d_ )
{}


template <typename T>
Dict<T>::Dict ( SchemaPtr sch )
    : DataObj( sch ),
      names( std::make_shared<std::vector<char>>() ),
      nip( std::make_shared<NameIdPair>( dflt_name, names ) ),
      d( std::make_shared<Hset>( 256, V{},  *nip, *nip ) )
{}


template <typename T>
Dict<T>::Dict ( SchemaPtr sch, const std::string& path )
    : DataObj( sch ),
      names( std::make_shared<std::vector<char>>() ),
      nip( std::make_shared<NameIdPair>( dflt_name, names ) ),
      d( std::make_shared<Hset>( 256, V{},  *nip, *nip ) ),
      d_path( path )
{}


template <typename T>
typename Dict<T>::Reference& Dict<T>::Reference::operator= ( T value )
{
    const char* local_name = _key.c_str();
    Id s_id = _nip->local_name( local_name );
    auto iter = _d->find( V( s_id, value ) );
    if ( !iter.second ) {
        int  id = _names->size();
        _names->insert( _names->end(), local_name, local_name + strlen( local_name ) + 1 );
        _d->insert( V( id, value ) );
    }
    else
        const_cast<V&>( iter.first ).second = value;
    return *this;
}


template <typename T>
Dict<T>::Reference::operator T () const
{
    const char*  local_name = _key.c_str();
    auto iter = _d->find( V( _nip->local_name( local_name ), T{} ) );
    if (iter.second)
        return iter.first.second;
    else
        throw Util::Error( "key \"%s\" not present in dictionary", local_name );
}


// template <typename T>
// std::ostream&  operator<< ( std::ostream& s, const typename Dict<T>::Reference& c )
// {
//     const char* local_name = c._key.c_str();
//     Id s_id = c._nip->local_name( local_name );
//     auto iter = c._d->find( V( s_id, T{} ) );
//     if ( !iter.second ) throw Util::Error( "Dict with key: \"%s\" is not existed! Please check the key name!\n",
//                                            local_name );
//     else return s << iter.first.second;
// }


template <typename T>
typename Dict<T>::StringId  Dict<T>::insert ( const std::string& key, T value )
{
    const char* local_name = key.c_str();
    Id s_id = nip->local_name( local_name );
    auto  search = d->find( V( s_id, value ) );
    if ( !search.second ) {
        StringId  id = names->size();
        names->insert( names->end(), local_name, local_name + strlen( local_name ) + 1 );
        return d->insert2( V( id, value ) ).first;
    }
    else {
        const_cast<V&>( search.first ).second = value;
        return search.first.first; // ?
    }
}


template <typename T>
bool Dict<T>::contains ( const std::string& key ) const
{
    const char* local_name = key.c_str();
    // TODO: mutex lock
    StringId s_id = nip->local_name( local_name );
    return d->contains( V( s_id, T{} ) );
    // mutex unlock
}


template <typename T>
const T&  Dict<T>::lookup ( const std::string& key, const T& dflt ) const
{
    auto non_const_this = const_cast<Dict<T>*> ( this );
    Id s_id = non_const_this->nip->local_name( key.c_str() );
    auto iter = non_const_this->d->find( V( s_id, T{} ) );
    if ( iter.second )
        return iter.first.second;
    else
        return dflt;
}


template <typename T>
IdType  Dict<T>::string_id ( const std::string& key ) const
{
    auto non_const_this = const_cast<Dict<T>*> ( this );
    Id s_id = non_const_this->nip->local_name( key.c_str() );
    auto iter = non_const_this->d->find2( V( s_id, T{} ) );
    return iter.first;
    // if ( iter.second )
    //     return iter.first.first;
    // else
    //     return ~0;
}


template <typename T>
const T&  Dict<T>::operator[] ( const std::string& key ) const
{
    auto non_const_this = const_cast<Dict<T>*> ( this );
    Id s_id = non_const_this->nip->local_name( key.c_str() );
    auto iter = non_const_this->d->find( V( s_id, T{} ) );
    if ( iter.second )
        return iter.first.second;
    else
        throw Util::Error( "dictionary key error: '%s' not found", key.c_str() );
}


template <typename T>
const T&  Dict<T>::operator[] ( IdType id ) const
{
    auto  search = d->at(id);
    if (search.second)
        return search.first;
    else
        throw Util::Error( "dictionary index error: '%d' not valid", id );
}


template <typename T>
typename Dict<T>::iterator Dict<T>::find ( const std::string& key )
{
    const char* local_name = key.c_str();
    Id s_id = nip->local_name( local_name );
    auto iter = d->find( V( s_id, T{} ) );
    if ( iter.second ) {
        Id index = d->get_hash_table()[ ( *nip )( iter.first ) % d->bucket_count() ];
        Iter pos = d->get_hash_data().begin() + index;
        return iterator( pos, *this );
    }
    else
        return this->end();
}


template <typename T>
GenericValue  Dict<T>::gvget ( const char* key ) const
{
    auto non_const_this = const_cast<Dict<T>*> ( this );
    Id s_id = non_const_this->nip->local_name( key );
    auto iter = non_const_this->d->find( V( s_id, T{} ) );
    if ( iter.second )
        return GenericValue( iter.first.second );
    else
        throw Util::Error( "The dictionary data with key \"%s\" is not existed!\n", key );
}


template <typename T>
GenericValue  Dict<T>::gvget ( int id ) const
{
    auto  x = d->at(id);
    if (x.second)
        return GenericValue( x.first.second );
    else throw Util::Error( "dictionary index out of bounds" );
}


template <typename T>
std::string  Dict<T>::key_at ( IdType id ) const
{
    auto  x = d->at(id);
    if (x.second)
        return std::string( names->data() + x.first.first );
    else throw Util::Error( "dictionary index out of bounds" );
}


template <typename T>
int  Dict<T>::gvset ( const char* key, GenericValue val )
{
    return this->insert( key, val.get<T>() );
}


template <typename T>
DataObjPtr  Dict<T>::slice ( size_t offset, size_t slice_size )
{
    DictData<T>* zero_copy_dict_data = new DictData<T>( this->schema(), names, nip, d );
    zero_copy_dict_data->d_parent = d_parent;
    return DataObjPtr( zero_copy_dict_data );
}


template <typename T>
DataObjPtr Dict<T>::slice_copy ( size_t offset, size_t slice_size )
{
    SchemaPtr sch = SchemaPtr( this->schema()->copy() );
    std::shared_ptr<std::vector<char>> names_ = std::make_shared<std::vector<char>>( *names );
    std::shared_ptr<NameIdPair> nip_ = std::make_shared<NameIdPair>( this->dflt_name, names_ );
    std::shared_ptr<Hset> d_ = std::make_shared<Hset>( *d ) ;
    d_->set_hasher( *nip_ );
    d_->set_comper( *nip_ );
    Dict<T>* copy_dict_data = new Dict<T>( sch, names_, nip_, d_ );
    return DataObjPtr( copy_dict_data );
}


template <typename T>
void  Dict<T>::serialize ( MPSys::Message& msg, DataObjPtr p )
{
    msg.frame( MPSys::make_frame< VectorReferenceData<std::vector<char>> >( *names, Defer(p) ));
    msg.frame( MPSys::make_frame< StaticMemoryData >( d->get_hash_info(), Defer(p) ));
    msg.frame( MPSys::make_frame< VectorReferenceData<std::vector<int>> >( d->get_hash_table(), Defer(p) ));
    msg.frame( MPSys::make_frame< VectorReferenceData<std::vector<typename Hset::Entry>> >( d->get_hash_data(), Defer(p)  ));
}


template<typename T>
void  Dict<T>::unload ()
{
    names = std::make_shared< std::vector<char> >();
    nip   = std::make_shared<NameIdPair>( dflt_name, names );
    d     = std::make_shared<Hset>( 256, V{},  *nip, *nip );
    d_deferral->unload( *this );
}


template <typename T>
void  Dict<T>::print ( int level ) const
{
    this->repr(std::cout, level); std::cout<<std::flush;
}

template<typename T>
void Dict<T>::repr (std::ostream& s, int level) const
{
    std::cout << Util::indent( level ) << schema()->name() << " Dict[" << size() << "] = [";
    size_t  limit = DataObj::s_print_limit;
    size_t count = 0;
    auto non_const_this = const_cast<Dict<T>*> ( this );
    for ( typename Hset::Iterator it( *( non_const_this->d ) );  it();  ++it ) {
        count++;
        if ( count == limit ) {
            std::cout << " ...";
            break;
        }

        std::cout << " \"" << nip->name( ( *it ).first ) << "\" : " << ( *it ).second;
        if ( count < size() ) std::cout << ",";

    }
    std::cout << " ]\n" << std::flush;
}
template <typename T>
void  Dict<T>::set_reference_ptrs ( DataObjPtr this_node )
{
    if ( not d_path.empty() ) {
        assert( not d_reference );
        d_reference = this->find_path( d_path );
    }
}


} // namespace AuData


template <typename T, typename Hasher, typename Comper>
void HashSet<T, Hasher, Comper>::rehash(int new_hash_size) {
    // Create new table and entries vector
    std::vector<int> new_table(new_hash_size, EOL);
    std::vector<Entry> new_entries;
    new_entries.reserve(d.size); // Reserve space for non-removed entries

    // Rehash and compact entries
    for (const auto& entry : d_entries) {
        if (entry.next >= EOL) {
            size_t h = d_hasher(entry.val) % new_hash_size;
            new_entries.emplace_back(entry.val, new_table[h]);
            new_table[h] = new_entries.size() - 1;
        }
    }

    // Swap the new table and entries with the old ones
    d_table.swap(new_table);
    d_entries.swap(new_entries);
    d.hash_size = new_hash_size;
}

template <typename T, typename Hasher, typename Comper>
bool HashSet<T, Hasher, Comper>::remove(const T& v) {
    size_t h = d_hasher(v) % d.hash_size;
    for (Int* i = d_table.data() + h; *i != EOL; i = &(d_entries[*i].next)) {
        Entry& e = d_entries[*i];
        if (d_comper(v, e.val)) {
            e.next = -2; // Mark as deleted
            d.size--;
            return true;
        }
    }
    return false;
}

template <typename T, typename Hasher, typename Comper>
void HashSet<T, Hasher, Comper>::compact() {
    std::vector<Entry> new_entries;
    new_entries.reserve(d.size); // Reserve space for non-removed entries

    for (const auto& entry : d_entries) {
        if (entry.next >= EOL) {
            new_entries.push_back(entry);
        }
    }

    d_entries.swap(new_entries);
}

if (d.size / d.hash_size > d.max_depth || d.size < d_entries.size() / 2) {
    this->compact();
}