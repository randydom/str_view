#pragma once

#include <string>
#include <atomic>
#include <algorithm> // for min, max
#include <memory> // for memcmp

#include <cassert>
#include <cstring>
#include <cstdint>

inline size_t tstrlen(const char* sz) { return strlen(sz); }
inline size_t tstrlen(const wchar_t* sz) { return wcslen(sz); }
inline void tstrcpy(char* dst, size_t dstCapacity, const char* src) { strcpy_s(dst, dstCapacity, src); }
inline void tstrcpy(wchar_t* dst, size_t dstCapacity, const wchar_t* src) { wcscpy_s(dst, dstCapacity, src); }

template<typename CharT>
class str_view_template
{
public:
    typedef CharT CharT;
    typedef std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>> StringT;

    // Initializes to empty string.
    inline str_view_template();
    
    // Initializes from a null-terminated string.
    // Null is acceptable. It means empty string.
    inline str_view_template(const CharT* sz);
    // Initializes from not null-terminated string.
    // Null is acceptable if length is 0.
    inline str_view_template(const CharT* str, size_t length);
    // Initializes from string with given length, with explicit statement that it is null-terminated.
    // Null is acceptable if length is 0.
    struct StillNullTerminated { };
    inline str_view_template(const CharT* str, size_t length, StillNullTerminated);
    
    // Initializes from an STL string.
    // length can exceed actual str.length(). It then spans to the end of str.
    inline str_view_template(const StringT& str, size_t offset = 0, size_t length = SIZE_MAX);

    // Copy constructor.
    inline str_view_template(const str_view_template<CharT>& src, size_t offset = 0, size_t length = SIZE_MAX);
    // Move constructor.
    inline str_view_template(str_view_template<CharT>&& src);
    
    inline ~str_view_template();

    // Copy assignment operator.
    inline str_view_template<CharT>& operator=(const str_view_template<CharT>& src);
    // Move assignment operator.
    inline str_view_template<CharT>& operator=(str_view_template<CharT>&& src);

    /*
    Returns the number of characters in the view. 
    */
    inline size_t length() const;
    /*
    Returns the number of characters in the view. 
    Usage of this method is not recommended because its name may be misleading -
    it may suggest size in bytes not in characters.
    */
    inline size_t size() const { return length(); }
    /*
    Checks if the view has no characters, i.e. whether length() == 0.
    It may be more efficient than checking length().
    */
    inline bool empty() const;
    /*
    Returns a pointer to the underlying character array.
    The pointer is such that the range [data(); data() + length()) is valid and the values in it
    correspond to the values of the view. 
    If empty() == true, returned pointer may or may not be null.
    */
    inline const CharT* data() const { return m_Begin; }
    /*
    Returns an iterator to the first character of the view.
    If empty() == true, returned pointer may or may not be null, but always begin() == end().
    */
    inline const CharT* begin() const { return m_Begin; }
    /*
    Returns an iterator to the character following the last character of the view.
    This character acts as a placeholder, attempting to access it results in undefined behavior. 
    */
    inline const CharT* end() const { return m_Begin + length(); }
    /*
    Returns reference to the first character in the view.
    The behavior is undefined if empty() == true.
    */
    inline const CharT* front() const { return m_Begin; }
    /*
    Returns reference to the last character in the view.
    The behavior is undefined if empty() == true. 
    */
    inline const CharT* back() const { return m_Begin + (length() - 1); }
    inline CharT operator[](size_t index) const { return m_Begin[index]; }
    inline CharT at(size_t index) const { return m_Begin[index]; }

    // Returns null-terminated string with contents of this object.
    // Possibly an internal copy.
    inline const CharT* c_str() const;

    /*
    Returns a view of the substring [offset, offset + length).
    length can exceed actual length(). It then spans to the end of this string.
    */
    inline str_view_template<CharT> substr(size_t offset = 0, size_t length = SIZE_MAX);

    /*
    Copies the substring [offset, offset + length) to the character string pointed to by dst.
    If pointer string ends before length is reached, string is copied to the end.
    Null character is not added past the end of destination.
    Returns number of characters copied.
    */
    inline size_t copy_to(CharT* dst, size_t offset = 0, size_t length = SIZE_MAX);

    inline void to_string(StringT& dst) { dst.assign(begin(), end()); }

    inline bool operator==(const str_view_template<CharT>& rhs) const;
    inline bool operator!=(const str_view_template<CharT>& rhs) const { return !operator==(rhs); }

private:
    // SIZE_MAX means unknown.
    mutable std::atomic<size_t> m_Length;
    const CharT* m_Begin;
    // 1 means pointed string is null-terminated by itself.
    // Any others bits set mean pointer to array with null-terminated copy.
    mutable std::atomic<uintptr_t> m_NullTerminatedPtr;
};

typedef str_view_template<char> str_view;
typedef str_view_template<wchar_t> wstr_view;

template<typename CharT>
inline str_view_template<CharT>::str_view_template() :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(0)
{
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const CharT* sz) :
	m_Length(sz ? SIZE_MAX : 0),
	m_Begin(sz),
	m_NullTerminatedPtr(sz ? 1 : 0)
{
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const CharT* str, size_t length) :
	m_Length(length),
	m_Begin(length ? str : nullptr),
	m_NullTerminatedPtr(0)
{
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const CharT* str, size_t length, StillNullTerminated) :
	m_Length(length),
	m_Begin(nullptr),
	m_NullTerminatedPtr(0)
{
    if(length)
    {
        m_Begin = str;
        m_NullTerminatedPtr = 1;
    }
    assert(m_Begin[m_Length] == (CharT)0); // Make sure it's really null terminated.
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const StringT& str, size_t offset, size_t length) :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(0)
{
	assert(offset <= str.length());
    m_Length = std::min(length, str.length() - offset);
    if(m_Length)
    {
        if(m_Length == str.length() - offset)
        {
            m_Begin = str.c_str() + offset;
            m_NullTerminatedPtr = 1;
        }
        else
            m_Begin = str.data() + offset;
    }
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const str_view_template<CharT>& src, size_t offset, size_t length) :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(0)
{
    // Source length is unknown, constructor doesn't limit the length - it may remain unknown.
    if(src.m_Length == SIZE_MAX && length == SIZE_MAX)
    {
        m_Length = SIZE_MAX;
        m_Begin = src.m_Begin + offset;
        assert(src.m_NullTerminatedPtr == 1);
        m_NullTerminatedPtr = 1;
    }
    else
    {
        const size_t srcLen = src.length();
	    assert(offset <= srcLen);
        m_Length = std::min(length, srcLen - offset);
        if(m_Length)
        {
            m_Begin = src.m_Begin + offset;
            if(src.m_NullTerminatedPtr == 1 && m_Length == srcLen - offset)
                m_NullTerminatedPtr = 1;
        }
    }
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(str_view_template<CharT>&& src) :
	m_Length(src.m_Length.exchange(0)),
	m_Begin(src.m_Begin),
	m_NullTerminatedPtr(src.m_NullTerminatedPtr.exchange(0))
{
	src.m_Begin = nullptr;
}

template<typename CharT>
inline str_view_template<CharT>::~str_view_template()
{
    uintptr_t v = m_NullTerminatedPtr;
	if(v > 1)
		delete[] (CharT*)v;
}

template<typename CharT>
inline str_view_template<CharT>& str_view_template<CharT>::operator=(const str_view_template<CharT>& src)
{
	if(&src != this)
    {
        uintptr_t v = m_NullTerminatedPtr;
		if(v > 1)
			delete[] (CharT*)v;
		m_Begin = src.m_Begin;
		m_Length = src.m_Length.load();
		m_NullTerminatedPtr = src.m_NullTerminatedPtr == 1 ? 1 : 0;
    }
	return *this;
}

template<typename CharT>
inline str_view_template<CharT>& str_view_template<CharT>::operator=(str_view_template<CharT>&& src)
{
	if(&src != this)
    {
        uintptr_t v = m_NullTerminatedPtr;
		if(v > 1)
			delete[] (CharT*)v;
		m_Begin = src.m_Begin;
		m_Length = src.m_Length.exchange(0);
		m_NullTerminatedPtr = src.m_NullTerminatedPtr.exchange(0);
		src.m_Begin = nullptr;
    }
	return *this;
}

template<typename CharT>
inline size_t str_view_template<CharT>::length() const
{
    size_t len = m_Length;
    if(len == SIZE_MAX)
    {
        assert(m_NullTerminatedPtr == 1);
        len = tstrlen(m_Begin);
        // It doesn't matter if other thread does it at the same time.
        // It will atomically set it to the same value.
        m_Length = len;
    }
    return len;
}

template<typename CharT>
inline bool str_view_template<CharT>::empty() const
{
    size_t len = m_Length;
    if(len == SIZE_MAX)
    {
        // Length is unknown. String is null-terminated.
        // We still don't need to know the length. We just peek first character.
        assert(m_NullTerminatedPtr == 1);
        return m_Begin == nullptr || *m_Begin == (CharT)0;
    }
    return len == 0;
}

template<typename CharT>
inline const CharT* str_view_template<CharT>::c_str() const
{
    static const CharT nullChar = (CharT)0;
	if(empty())
		return &nullChar;
    uintptr_t v = m_NullTerminatedPtr;
	if(v == 1)
    {
        assert(m_Begin[length()] == (CharT)0); // Make sure it's really null terminated.
		return m_Begin;
    }
	if(v == 0)
    {
        // Not null terminated, so length must be known.
        assert(m_Length != SIZE_MAX);
        CharT* nullTerminatedCopy = new CharT[m_Length + 1];
        assert(((uintptr_t)nullTerminatedCopy & 1) == 0); // Make sure allocated address is even.
		memcpy(nullTerminatedCopy, begin(), m_Length * sizeof(CharT));
		nullTerminatedCopy[m_Length] = (CharT)0;

        uintptr_t expected = 0;
        if(m_NullTerminatedPtr.compare_exchange_strong(expected, (uintptr_t)nullTerminatedCopy))
            return nullTerminatedCopy;
        else
        {
            // Other thread was quicker to set his copy to m_NullTerminatedPtr. Destroy mine, use that one.
            delete[] nullTerminatedCopy;
            return (const CharT*)expected;
        }
    }
	return (const CharT*)v;
}

template<typename CharT>
inline size_t str_view_template<CharT>::copy_to(CharT* dst, size_t offset, size_t length)
{
    const size_t thisLen = this->length();
    assert(offset <= thisLen);
    length = std::min(length, thisLen - offset);
    memcpy(dst, m_Begin + offset, length * sizeof(CharT));
    return length;
}

template<typename CharT>
inline str_view_template<CharT> str_view_template<CharT>::substr(size_t offset, size_t length)
{
    // Length can remain unknown.
    if(m_Length == SIZE_MAX && length == SIZE_MAX)
    {
        assert(m_NullTerminatedPtr == 1);
        return str_view_template<CharT>(m_Begin + offset);
    }

    const size_t thisLen = this->length();
    assert(offset <= thisLen);
	length = std::min(length, thisLen - offset);
	// Result will be null-terminated.
	if(m_NullTerminatedPtr == 1 && length == thisLen - offset)
		return str_view_template<CharT>(m_Begin + offset, length, StillNullTerminated());
	// Result will not be null-terminated.
	return str_view_template<CharT>(m_Begin + offset, length);
}

template<typename CharT>
inline bool str_view_template<CharT>::operator==(const str_view_template<CharT>& rhs) const
{
    const size_t thisLen = length();
    if(thisLen != rhs.length())
        return false;
    if(thisLen > 0)
    {
        return memcmp(data(), rhs.data(), thisLen * sizeof(CharT)) == 0;
    }
    return true;
        
}