
namespace pystring {

// @mainpage pystring
//
// This is a set of functions matching the interface and behaviors of python string methods
// (as of python 2.3) using std::string.
//
// Overlapping functionality ( such as index and slice/substr ) of std::string is included
// to match python interfaces.
//

/*
    	* @defgroup functions pystring
    	* @{
    	*/

// @brief Return a copy of the string with only its first character capitalized.
//
template <class S>
S capitalize(const S& str);

///
// @brief Return centered in a string of length width. Padding is done using spaces.
//
string center(const string_view& str, std::ptrdiff_t width);

//
// @brief Return the number of occurrences of substring sub in string S[start:end]. Optional
// arguments start and end are interpreted as in slice notation.
//
template <class S>
std::ptrdiff_t count(const S& str, const S& substr, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<std::ptrdiff_t>::max());

// @brief Return True if the string ends with the specified suffix, otherwise return False. With
// optional start, test beginning at that position. With optional end, stop comparing at that position.
//
template <class S, class C>
bool endswith(const S& str, const C& suffix, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<std::ptrdiff_t>::max());

///
// @brief Return a copy of the string where all tab characters are expanded using spaces. If tabsize
// is not given, a tab size of 8 characters is assumed.
//
string expandtabs(const string_view& str, std::ptrdiff_t tabsize = 8);

///
// @brief Return the lowest index in the string where substring sub is found, such that sub is
// contained in the range [start, end). Optional arguments start and end are interpreted as
// in slice notation. Return -1 if sub is not found.
//
template <class S>
std::ptrdiff_t find(const string_view& str, const S& sub, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<std::ptrdiff_t>::max());

///
// @brief Synonym of find right now. Python version throws exceptions. This one currently doesn't
//
template <class S>
std::ptrdiff_t index(const string_view& str, const S& sub, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<std::ptrdiff_t>::max());

///
// @brief Return true if all characters in the string are alphanumeric and there is at least one
// character, false otherwise.
//
template <class S>
bool isalnum(const S& str);

///
// @brief Return true if all characters in the string are alphabetic and there is at least one
// character, false otherwise
//
template <class S>
bool isalpha(const S& str);

///
// @brief Return true if all characters in the string are digits and there is at least one
// character, false otherwise.
//
template <class S>
bool isdigit(const S& str);

///
// @brief Return true if all cased characters in the string are lowercase and there is at least one
// cased character, false otherwise.
//
template <class S>
bool islower(const S& str);

///
// @brief Return true if there are only whitespace characters in the string and there is at least
// one character, false otherwise.
//
template <class S>
bool isspace(const S& str);

///
// @brief Return true if the string is a titlecased string and there is at least one character,
// i.e. uppercase characters may only follow uncased characters and lowercase characters only
// cased ones. Return false otherwise.
//
template <class S>
bool istitle(const S& str);

///
// @brief Return true if all cased characters in the string are uppercase and there is at least one
// cased character, false otherwise.
//
template <class S>
bool isupper(const S& str);

///
// @brief Return a string which is the concatenation of the strings in the sequence seq.
// The separator between elements is the str argument
//
template <class S, class C>
string join(const S& str, const C& seq);

///
// @brief Return the string left justified in a string of length width. Padding is done using
// spaces. The original string is returned if width is less than str.size().
//
string ljust(const string_view& str, std::ptrdiff_t width);

///
// @brief Return a copy of the string converted to lowercase.
//
template <class S>
S lower(const S& str);

///
// @brief Return a copy of the string with leading characters removed. If chars is omitted or None,
// whitespace characters are removed. If given and not "", chars must be a string; the
// characters in the string will be stripped from the beginning of the string this method
// is called on (argument "str" ).
//
string_view lstrip(const string_view& str, const string_view& chars = "");

///
// @brief Return a copy of the string, concatenated N times, together.
// Corresponds to the __mul__ operator.
//
template <class S>
S mul(const S& str, std::ptrdiff_t n);

///
// @brief Split the string around first occurance of sep.
// Three strings will always placed into result. If sep is found, the strings will
// be the text before sep, sep itself, and the remaining text. If sep is
// not found, the original string will be returned with two empty strings.
//
template <class S>
std::vector<S> partition(const S& str, const S& sep);

///
// @brief Return a copy of the string with all occurrences of substring old replaced by new. If
// the optional argument count is given, only the first count occurrences are replaced.
//
template <class S, class T, class U>
string replace(const S& str, const T& oldstr, const U& newstr, std::ptrdiff_t count = -1);

///
// @brief Return the highest index in the string where substring sub is found, such that sub is
// contained within s[start,end]. Optional arguments start and end are interpreted as in
// slice notation. Return -1 on failure.
//
template <class S>
std::ptrdiff_t rfind(const string_view& str, const S& sub, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<int>::max());

///
// @brief Currently a synonym of rfind. The python version raises exceptions. This one currently
// does not
//
template <class S>
std::ptrdiff_t rindex(const string_view& str, const S& sub, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<int>::max());

///
// @brief Return the string right justified in a string of length width. Padding is done using
// spaces. The original string is returned if width is less than str.size().
//
string rjust(const string_view& str, std::ptrdiff_t width);

///
// @brief Split the string around last occurance of sep.
// Three strings will always placed into result. If sep is found, the strings will
// be the text before sep, sep itself, and the remaining text. If sep is
// not found, the original string will be returned with two empty strings.
//
std::array<string_view, 3> rpartition(const string_view& str, const string_view& sep);

///
// @brief Return a copy of the string with trailing characters removed. If chars is "", whitespace
// characters are removed. If not "", the characters in the string will be stripped from the
// end of the string this method is called on.
//
string_view rstrip(const string_view& str, const string_view& chars = "");

///
// @brief Fills the "result" list with the words in the string, using sep as the delimiter string.
// If maxsplit is > -1, at most maxsplit splits are done. If sep is "",
// any whitespace string is a separator.
//
std::vector<string_view> split(const string_view& str, const string_view& sep = "", std::ptrdiff_t maxsplit = -1);

///
// @brief Fills the "result" list with the words in the string, using sep as the delimiter string.
// Does a number of splits starting at the end of the string, the result still has the
// split strings in their original order.
// If maxsplit is > -1, at most maxsplit splits are done. If sep is "",
// any whitespace string is a separator.
//
std::vector<string_view> rsplit(const string_view& str, const string_view& sep = "", std::ptrdiff_t maxsplit = -1);

///
// @brief Return a list of the lines in the string, breaking at line boundaries. Line breaks
// are not included in the resulting list unless keepends is given and true.
//
std::vector<string_view> splitlines(const string_view& str, bool keepends = false);

///
// @brief Return True if string starts with the prefix, otherwise return False. With optional start,
// test string beginning at that position. With optional end, stop comparing string at that
// position
//
template <class S, class C>
bool startswith(const S& str, const C& prefix, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<int>::max());

///
// @brief Return a copy of the string with leading and trailing characters removed. If chars is "",
// whitespace characters are removed. If given not "",  the characters in the string will be
// stripped from the both ends of the string this method is called on.
//
string_view strip(const string_view& str, const string_view& chars = "");

///
// @brief Return a copy of the string with uppercase characters converted to lowercase and vice versa.
//
template <class S>
S swapcase(const S& str);

///
// @brief Return a titlecased version of the string: words start with uppercase characters,
// all remaining cased characters are lowercase.
//
template <class S>
S title(const S& str);

///
// @brief Return a copy of the string where all characters occurring in the optional argument
// deletechars are removed, and the remaining characters have been mapped through the given
// translation table, which must be a string of length 256.
//
template <class S>
string translate(const S& str, const string_view& table, const string_view& deletechars = "");

///
// @brief Return a copy of the string converted to uppercase.
//
template <class S>
S upper(const S& str);

///
// @brief Return the numeric string left filled with zeros in a string of length width. The original
// string is returned if width is less than str.size().
//
template <class S>
S zfill(const S& str, std::ptrdiff_t width);

///
// @brief function matching python's slice functionality.
//
string_view slice(const string_view& str, std::ptrdiff_t start = 0, std::ptrdiff_t end = std::numeric_limits<std::ptrdiff_t>::max());

//
// @ }
//

namespace os {
    namespace path {
        // All of the function below have three versions.
        // Example:
        //   join(...)
        //   join_nt(...)
        //   join_posix(...)
        //
        // The regular function dispatches to the other versions - based on the OS
        // at compile time - to match the result you'd get from the python
        // interepreter on the same operating system
        //
        // Should you want to 'lock off' to a particular version of the string
        // manipulation across *all* operating systems, use the version with the
        // _OS you are interested in.  I.e., you can use posix style path joining,
        // even on Windows, with join_posix.
        //
        // The naming, (nt, posix) matches the cpython source implementation.

        ///
        // @defgroup functions pystring::os::path
        // @{

        ///
        // @brief Return the base name of pathname path. This is the second half of the pair returned
        // by split(path). Note that the result of this function is different from the Unix basename
        // program; where basename for '/foo/bar/' returns 'bar', the basename() function returns an
        // empty string ('').

        std::string basename(const std::string& path);
        std::string basename_nt(const std::string& path);
        std::string basename_posix(const std::string& path);

        ///
        // @brief Return the directory name of pathname path. This is the first half of the pair
        // returned by split(path).

        std::string dirname(const std::string& path);
        std::string dirname_nt(const std::string& path);
        std::string dirname_posix(const std::string& path);

        ///
        // @brief Return True if path is an absolute pathname. On Unix, that means it begins with a
        // slash, on Windows that it begins with a (back)slash after chopping off a potential drive
        // letter.

        bool isabs(const std::string& path);
        bool isabs_nt(const std::string& path);
        bool isabs_posix(const std::string& s);

        ///
        // @brief Return a normalized absolutized version of the pathname path.
        //
        // NOTE: This differs from the interface of the python equivalent in that it requires you
        // to pass in the current working directory as an argument.

        std::string abspath(const std::string& path, const std::string& cwd);
        std::string abspath_nt(const std::string& path, const std::string& cwd);
        std::string abspath_posix(const std::string& path, const std::string& cwd);

        ///
        // @brief Join one or more path components intelligently. If any component is an absolute
        // path, all previous components (on Windows, including the previous drive letter, if there
        // was one) are thrown away, and joining continues. The return value is the concatenation of
        // path1, and optionally path2, etc., with exactly one directory separator (os.sep) inserted
        // between components, unless path2 is empty. Note that on Windows, since there is a current
        // directory for each drive, os.path.join("c:", "foo") represents a path relative to the
        // current directory on drive C: (c:foo), not c:\foo.

        // This dispatches based on the compilation OS
        std::string join(const std::string& path1, const std::string& path2);
        std::string join_nt(const std::string& path1, const std::string& path2);
        std::string join_posix(const std::string& path1, const std::string& path2);

        std::string join(const std::vector<std::string>& paths);
        std::string join_nt(const std::vector<std::string>& paths);
        std::string join_posix(const std::vector<std::string>& paths);

        ///
        // @brief Normalize a pathname. This collapses redundant separators and up-level references
        // so that A//B, A/B/, A/./B and A/foo/../B all become A/B. It does not normalize the case
        // (use normcase() for that). On Windows, it converts forward slashes to backward slashes.
        // It should be understood that this may change the meaning of the path if it contains
        // symbolic links!

        std::string normpath(const std::string& path);
        std::string normpath_nt(const std::string& path);
        std::string normpath_posix(const std::string& path);

        ///
        // @brief Split the pathname path into a pair, (head, tail) where tail is the last pathname
        // component and head is everything leading up to that. The tail part will never contain a
        // slash; if path ends in a slash, tail will be empty. If there is no slash in path, head
        // will be empty. If path is empty, both head and tail are empty. Trailing slashes are
        // stripped from head unless it is the root (one or more slashes only). In all cases,
        // join(head, tail) returns a path to the same location as path (but the strings may
        // differ).

        void split(std::string& head, std::string& tail, const std::string& path);
        void split_nt(std::string& head, std::string& tail, const std::string& path);
        void split_posix(std::string& head, std::string& tail, const std::string& path);

        ///
        // @brief Split the pathname path into a pair (drive, tail) where drive is either a drive
        // specification or the empty string. On systems which do not use drive specifications,
        // drive will always be the empty string. In all cases, drive + tail will be the same as
        // path.

        void splitdrive(std::string& drivespec, std::string& pathspec, const std::string& path);
        void splitdrive_nt(std::string& drivespec, std::string& pathspec, const std::string& p);
        void splitdrive_posix(std::string& drivespec, std::string& pathspec, const std::string& path);

        ///
        // @brief Split the pathname path into a pair (root, ext) such that root + ext == path, and
        // ext is empty or begins with a period and contains at most one period. Leading periods on
        // the basename are ignored; splitext('.cshrc') returns ('.cshrc', '').

        void splitext(std::string& root, std::string& ext, const std::string& path);
        void splitext_nt(std::string& root, std::string& ext, const std::string& path);
        void splitext_posix(std::string& root, std::string& ext, const std::string& path);
    }

} // namespace path
} // namespace os
