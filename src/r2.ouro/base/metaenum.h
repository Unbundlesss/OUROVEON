//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

// pile of macros that let you define an enum that is more strongly typed (wrote this before 'enum class')
// and modestly "reflectable" - you can convert entries to strings, and vice versa, plus you can 'get next' to move
// down an ordered list of states (eg. Phase1, Phase2, Phase3 etc)
//
// #define _ENUM_ENTRIES(_entry)  \
//    _entry(Something)           \
//    _entry(Else)                \
//    _entry(Instead)             \
//    _entry(Because)             \
//    _entry(Example)
// 
// REFLECT_ENUM(Thing, uint32_t, _ENUM_ENTRIES);
//
// #undef _ENUM_ENTRIES
//
// Thing::Enum et = Thing::Something;
// et  = Thing::FromString("Instead");
//
// et  = Thing::GetNext(et);
// et == Thing::Because;
//
// FOREACH( Thing, t ) 
// {
//   printf( "%s", Thing::ToString(t) );
// }
//

#define META_MACRO_JOIN(_lhs, _rhs)       META_MACRO_DO_JOIN(_lhs, _rhs)
#define META_MACRO_DO_JOIN(_lhs, _rhs)    META_MACRO_DO_JOIN2(_lhs, _rhs)
#define META_MACRO_DO_JOIN2(_lhs, _rhs)   _lhs##_rhs

namespace const_str
{

// ---------------------------------------------------------------------------------------------------------------------
constexpr int compare( const char* a, const char* b )
{
    return *a == 0 && *b == 0 ? 0 :
        *a == 0 ? -1 :
        *b == 0 ? 1 :
        *a < *b ? -1 :
        *a > *b ? 1 :
        *a == *b ? compare( a + 1, b + 1 ) :
        throw "strcmp_fail";
}

// ---------------------------------------------------------------------------------------------------------------------
// process an input string into a buffer, turning "SomeTitleCase" into "Some Title Case" .. with spacing
template< std::size_t _buflen >
inline void titleize( const char* tcin, char( &buffer )[_buflen] )
{
    const auto tcin_len = std::min( strlen( tcin ), _buflen - 1 );
    std::size_t buf_pt = 0;
    bool lastWasUpper = false;
    for ( auto i = 0; i < tcin_len; i++ )
    {
        if ( tcin[i] >= 'A' &&
            tcin[i] <= 'Z' )
        {
            if ( !lastWasUpper )
                buffer[buf_pt++] = ' ';
            lastWasUpper = true;
        }
        else
            lastWasUpper = false;

        buffer[buf_pt++] = tcin[i];
    }
    buffer[buf_pt] = '\0';
}

} // namespace const_str

// ---------------------------------------------------------------------------------------------------------------------
// these are the actions used within the enum creation macro to produce output per-entry
// we break out the _ID ones to avoid triggering errors when using Clang in -Wall mode
#define METAENUM_DEF_ENUM(_ty)                  _ty,
#define METAENUM_DEF_ENUM_ID(_ty, _id)          _ty = _id,
#define METAENUM_DEF_TOSTR(_ty)                 case _ty: return #_ty;
#define METAENUM_DEF_FIDX(_ty)                  case _ty: return _ty;
#define METAENUM_DEF_VALID(_ty)                 case _ty: return true;
#define METAENUM_DEF_FROMSTR(_ty)               if ( const_str::compare(str, #_ty ) == 0) return _ty; else 
#define METAENUM_DEF_NEXTWRAP(_ty)              return _ty; case _ty: 
#define METAENUM_DEF_NEXTVALID(_ty)             return true; case _ty: 

#define METAENUM_DEF_ENUM_ID_TYPE(_ty, ...)     METAENUM_DEF_ENUM(_ty)
#define METAENUM_DEF_TOSTR_ID(_ty, ...)         METAENUM_DEF_TOSTR(_ty)
#define METAENUM_DEF_FIDX_ID(_ty, ...)          METAENUM_DEF_FIDX(_ty)
#define METAENUM_DEF_VALID_ID(_ty, ...)         METAENUM_DEF_VALID(_ty)
#define METAENUM_DEF_FROMSTR_ID(_ty, ...)       METAENUM_DEF_FROMSTR(_ty)
#define METAENUM_DEF_NEXTWRAP_ID(_ty, ...)      METAENUM_DEF_NEXTWRAP(_ty)
#define METAENUM_DEF_NEXTVALID_ID(_ty, ...)     METAENUM_DEF_NEXTVALID(_ty)

#define METAENUM_STORAGE_SPEC(_storageType) 

// for-loop that walks an enum
#define META_FOREACH( _enumName, _iterV ) for ( _enumName::Enum _iterV = _enumName::getFirst(); _enumName::isValidValue(_iterV); _iterV = _enumName::getNext(_iterV, false) )


// ---------------------------------------------------------------------------------------------------------------------
// main code expander
// _enumName is the wrapper name for the enum, eg. VertexFormat or PlayerState
// _storageType is used as the C++11 storage class if available, along with setting the index type
// _enumAdorn pass-through for any declaration adornments, like 'abstract' or 'sealed' on MSVC
// _macroWorker is the macro that takes an action macro to create code. confused yet?
// _????Builder are the functions that create code specific to individual functions
// _topAccessor chooses what to call the accessor for the 'top' or 'count' depending if it has IDs or not
// 
#define _TL_CREATE_METAENUM(_enumName, _storageType, _ename, _macroWorker, _plainEntryBuilder, _entryBuilder, _fidxBuilder, _validBuilder, _toStrBuilder, _fromStrBuilder, _wrapNextBuilder, _nextValidBuilder, _topAccessor)     \
struct _enumName                                                                                                                                                                                             \
{                                                                                                                                                                                                            \
  typedef _storageType StorageType;                                                                                                                                                                          \
  enum  _ename : _storageType                                                                                                                                                                                \
  {                                                                                                                                                                                                          \
    _macroWorker(_entryBuilder)                                                                                                                                                                              \
    __EnumTerminator,                                                                                                                                                                                        \
    Unspecified                                                                                                                                                                                              \
  };                                                                                                                                                                                                         \
                                                                                                                                                                                                             \
  inline static constexpr const char* enumName() { return #_enumName; }                                                                                                                                      \
  enum Constants { _topAccessor = _ename::__EnumTerminator };                                                                                                                                                \
  inline static constexpr _storageType enum##_topAccessor() { return static_cast<_storageType>(_ename::__EnumTerminator); }                                                                                  \
                                                                                                                                                                                                             \
  inline static constexpr _enumName::_ename getByValue(_storageType idx)                                                                                                                                     \
  {                                                                                                                                                                                                          \
    switch (idx)                                                                                                                                                                                             \
    {                                                                                                                                                                                                        \
      _macroWorker(_fidxBuilder)                                                                                                                                                                             \
      default:                                                                                                                                                                                               \
      {                                                                                                                                                                                                      \
      assert(0);                                                                                                                                                                                             \
      return (_enumName::_ename::__EnumTerminator);                                                                                                                                                          \
      }                                                                                                                                                                                                      \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
                                                                                                                                                                                                             \
  inline static constexpr bool isValidValue(_storageType idx)                                                                                                                                                \
  {                                                                                                                                                                                                          \
    switch ((_enumName::_ename)idx)                                                                                                                                                                          \
    {                                                                                                                                                                                                        \
      _macroWorker(_validBuilder)                                                                                                                                                                            \
      default:                                                                                                                                                                                               \
      return false;                                                                                                                                                                                          \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
                                                                                                                                                                                                             \
  inline static constexpr const char* toString(_enumName::_ename e)                                                                                                                                          \
  {                                                                                                                                                                                                          \
    switch (e)                                                                                                                                                                                               \
    {                                                                                                                                                                                                        \
      _macroWorker(_toStrBuilder)                                                                                                                                                                            \
      default:                                                                                                                                                                                               \
      {                                                                                                                                                                                                      \
        assert(0);                                                                                                                                                                                           \
        return ("error");                                                                                                                                                                                    \
      }                                                                                                                                                                                                      \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
                                                                                                                                                                                                             \
  inline static constexpr _enumName::_ename fromString(const char* str)                                                                                                                                      \
  {                                                                                                                                                                                                          \
    _macroWorker(_fromStrBuilder)                                                                                                                                                                            \
    {                                                                                                                                                                                                        \
      return (_enumName::_ename::Unspecified);                                                                                                                                                               \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
                                                                                                                                                                                                             \
  inline static constexpr _enumName::_ename getFirst()                                                                                                                                                       \
  {                                                                                                                                                                                                          \
    constexpr _enumName::_ename _ent_list[] = {                                                                                                                                                              \
      _macroWorker(_plainEntryBuilder)                                                                                                                                                                       \
      };                                                                                                                                                                                                     \
    return _ent_list[0];                                                                                                                                                                                     \
  }                                                                                                                                                                                                          \
  inline static constexpr _enumName::_ename getNext( _enumName::_ename e, const bool assert_on_overflow = true )                                                                                             \
  {                                                                                                                                                                                                          \
    switch (e)                                                                                                                                                                                               \
    {                                                                                                                                                                                                        \
      default:                                                                                                                                                                                               \
      _macroWorker(_wrapNextBuilder)                                                                                                                                                                         \
      if ( assert_on_overflow ) { assert(0); }                                                                                                                                                               \
      return Unspecified;                                                                                                                                                                                    \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
  inline static constexpr _enumName::_ename getNextWrapped( _enumName::_ename e )                                                                                                                            \
  {                                                                                                                                                                                                          \
    switch (e)                                                                                                                                                                                               \
    {                                                                                                                                                                                                        \
      default:                                                                                                                                                                                               \
      _macroWorker(_wrapNextBuilder)                                                                                                                                                                         \
      return getFirst();                                                                                                                                                                                     \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
  inline static constexpr bool hasNext( _enumName::_ename e )                                                                                                                                                \
  {                                                                                                                                                                                                          \
    switch (e)                                                                                                                                                                                               \
    {                                                                                                                                                                                                        \
      default:                                                                                                                                                                                               \
      _macroWorker(_nextValidBuilder)                                                                                                                                                                        \
      return false;                                                                                                                                                                                          \
    }                                                                                                                                                                                                        \
  }                                                                                                                                                                                                          \
private:                                                                                                                                                                                                     \
  _enumName();                                                                                                                                                                                               \
public:                                                                                                                                                                                                      \
    inline static bool ImGuiCombo( const char* label, _enumName::_ename& evalue )                                                                                                                            \
    {                                                                                                                                                                                                        \
        char titleCasingBuffer[128];                                                                                                                                                                         \
        bool wasChanged = false;                                                                                                                                                                             \
        const_str::titleize( toString( evalue ), titleCasingBuffer );                                                                                                                                        \
        if ( ImGui::BeginCombo( label, titleCasingBuffer, 0 ) )                                                                                                                                              \
        {                                                                                                                                                                                                    \
            META_FOREACH( _enumName, lb )                                                                                                                                                                    \
            {                                                                                                                                                                                                \
                const bool selected = (evalue == lb);                                                                                                                                                        \
                const_str::titleize( toString( lb ), titleCasingBuffer );                                                                                                                                    \
                if ( ImGui::Selectable( titleCasingBuffer, selected ) )                                                                                                                                      \
                {                                                                                                                                                                                            \
                    evalue = lb;                                                                                                                                                                             \
                    wasChanged = true;                                                                                                                                                                       \
                }                                                                                                                                                                                            \
                if ( selected )                                                                                                                                                                              \
                    ImGui::SetItemDefaultFocus();                                                                                                                                                            \
            }                                                                                                                                                                                                \
            ImGui::EndCombo();                                                                                                                                                                               \
        }                                                                                                                                                                                                    \
        return wasChanged;                                                                                                                                                                                   \
    }                                                                                                                                                                                                        \
}

#define REFLECT_ENUM(_enumName, _storageType, _macroWorker) \
  _TL_CREATE_METAENUM(_enumName, _storageType, Enum, _macroWorker, METAENUM_DEF_ENUM, METAENUM_DEF_ENUM, METAENUM_DEF_FIDX, METAENUM_DEF_VALID, METAENUM_DEF_TOSTR, METAENUM_DEF_FROMSTR, METAENUM_DEF_NEXTWRAP, METAENUM_DEF_NEXTVALID, Count)

#define REFLECT_ENUM_CUSTOM_STRCONV(_enumName, _storageType, _macroWorker, _toStr, _fromStr) \
  _TL_CREATE_METAENUM(_enumName, _storageType, Enum, _macroWorker, METAENUM_DEF_ENUM, METAENUM_DEF_ENUM, METAENUM_DEF_FIDX, METAENUM_DEF_VALID, _toStr, _fromStr, METAENUM_DEF_NEXTWRAP, METAENUM_DEF_NEXTVALID, Count)

#define REFLECT_ENUM_IDS(_enumName, _storageType, _macroWorker) \
  _TL_CREATE_METAENUM(_enumName, _storageType, Enum, _macroWorker, METAENUM_DEF_ENUM_ID_TYPE, METAENUM_DEF_ENUM_ID, METAENUM_DEF_FIDX_ID, METAENUM_DEF_VALID_ID, METAENUM_DEF_TOSTR_ID, METAENUM_DEF_FROMSTR_ID, METAENUM_DEF_NEXTWRAP_ID, METAENUM_DEF_NEXTVALID_ID, Count)

#define REFLECT_ENUM_NAMED(_enumName, _storageType, _ename, _macroWorker) \
  _TL_CREATE_METAENUM(_enumName, _storageType, _ename, _macroWorker, METAENUM_DEF_ENUM, METAENUM_DEF_ENUM, METAENUM_DEF_FIDX, METAENUM_DEF_VALID, METAENUM_DEF_TOSTR, METAENUM_DEF_FROMSTR, METAENUM_DEF_NEXTWRAP, METAENUM_DEF_NEXTVALID, Count)




