#pragma once
// Small C++14 compatibility layer for the Nimple Basic runtime.
#include <string>
#include <vector>
#include <sstream>
#include <system_error>
#include <cstdlib>
#include <utility>
#include <cstddef>
#include <cerrno>
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#endif

namespace nbcompat {
struct nullopt_t {};
static const nullopt_t nullopt = nullopt_t();
template<class T> class optional {
    bool has_; T value_;
public:
    optional() : has_(false), value_() {}
    optional(nullopt_t) : has_(false), value_() {}
    optional(const T& v) : has_(true), value_(v) {}
    optional(T&& v) : has_(true), value_(std::move(v)) {}
    optional& operator=(nullopt_t) { has_ = false; value_ = T(); return *this; }
    optional& operator=(const T& v) { value_ = v; has_ = true; return *this; }
    bool has_value() const { return has_; }
    explicit operator bool() const { return has_; }
    T& operator*() { return value_; }
    const T& operator*() const { return value_; }
    T* operator->() { return &value_; }
    const T* operator->() const { return &value_; }
};
inline std::string narrow(const std::wstring& s) {
#ifdef _WIN32
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), 0, 0, 0, 0);
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, 0, 0);
    return out;
#else
    return std::string(s.begin(), s.end());
#endif
}
namespace fs {
class path {
    std::string p_;
    static std::string normalize(std::string s) {
#ifdef _WIN32
        std::replace(s.begin(), s.end(), '/', '\\'); const char sep='\\';
#else
        const char sep='/';
#endif
        std::string prefix; size_t pos=0;
#ifdef _WIN32
        if (s.size()>1 && s[1]==':') { prefix=s.substr(0,2); pos=2; }
#endif
        bool absolute = pos<s.size() && s[pos]==sep; if (absolute) ++pos;
        std::vector<std::string> parts; std::stringstream ss(s.substr(pos)); std::string x;
        while (std::getline(ss,x,sep)) { if (x.empty()||x==".") continue; if (x==".."&&!parts.empty()&&parts.back()!="..") parts.pop_back(); else if(x!=".."||!absolute) parts.push_back(x); }
        std::string out=prefix; if(absolute) out+=sep; for(size_t i=0;i<parts.size();++i){if(i)out+=sep;out+=parts[i];} return out.empty()?".":out;
    }
public:
    path() {}
    path(const char* s):p_(s?s:""){}
    path(const std::string& s):p_(s){}
    path(const std::wstring& s):p_(narrow(s)){}
    path& operator=(const std::string& s){p_=s;return *this;}
    std::string string() const { return p_; }
    operator std::string() const { return p_; }
    std::wstring wstring() const {
#ifdef _WIN32
        if (p_.empty()) return std::wstring();
        int n=MultiByteToWideChar(CP_UTF8,0,p_.c_str(),(int)p_.size(),0,0);
        std::wstring out((size_t)n,L'\0');
        if(n>0) MultiByteToWideChar(CP_UTF8,0,p_.c_str(),(int)p_.size(),&out[0],n);
        return out;
#else
        return std::wstring(p_.begin(),p_.end());
#endif
    }
    std::string u8string() const { return p_; }
    bool empty() const { return p_.empty(); }
    bool is_relative() const { return !is_absolute(); }
    bool is_absolute() const {
#ifdef _WIN32
        return (p_.size()>2 && p_[1]==':' ) || (!p_.empty()&&(p_[0]=='\\'||p_[0]=='/'));
#else
        return !p_.empty()&&p_[0]=='/';
#endif
    }
    bool has_parent_path() const { return p_.find_last_of("/\\")!=std::string::npos; }
    path parent_path() const { size_t n=p_.find_last_of("/\\"); if(n==std::string::npos)return path(); if(n==0)return path(p_.substr(0,1)); return path(p_.substr(0,n)); }
    path filename() const { size_t n=p_.find_last_of("/\\"); return path(n==std::string::npos?p_:p_.substr(n+1)); }
    path extension() const { std::string f=filename().p_; size_t n=f.find_last_of('.'); return n==std::string::npos?path():path(f.substr(n)); }
    void replace_extension(const std::string& e) { size_t slash=p_.find_last_of("/\\"), dot=p_.find_last_of('.'); if(dot==std::string::npos||(slash!=std::string::npos&&dot<slash)) p_+=e; else p_=p_.substr(0,dot)+e; }
    path lexically_normal() const { return path(normalize(p_)); }
    friend bool operator==(const path& a,const path& b){return a.p_==b.p_;}
    friend bool operator!=(const path& a,const path& b){return !(a==b);}
    friend bool operator==(const path& a,const char* b){return a.p_==(b?b:"");}
    friend bool operator!=(const path& a,const char* b){return !(a==b);}
    friend path operator/(const path& a,const path& b) { if(b.is_absolute())return b; if(a.p_.empty())return b; std::string s=a.p_; if(s.back()!='/'&&s.back()!='\\')s+='/'; s+=b.p_; return path(s); }
};
inline path u8path(const std::string& s){return path(s);} inline path u8path(const char* s){return path(s);}
inline path absolute(const path& p,std::error_code& ec) {
#ifdef _WIN32
    wchar_t buf[32768]; const std::wstring input=p.wstring(); DWORD n=GetFullPathNameW(input.c_str(),32768,buf,0); if(!n||n>=32768){ec=std::error_code((int)GetLastError(),std::system_category());return p;} ec.clear(); return path(std::wstring(buf,n));
#else
    char buf[PATH_MAX]; if(realpath(p.string().c_str(),buf)){ec.clear();return path(buf);} if(p.is_absolute()){ec.clear();return p.lexically_normal();} char cwd[PATH_MAX]; if(!getcwd(cwd,sizeof(cwd))){ec=std::error_code(errno,std::generic_category());return p;} ec.clear();return (path(cwd)/p).lexically_normal();
#endif
}
inline path absolute(const path& p){std::error_code ec;return absolute(p,ec);}
inline path weakly_canonical(const path& p,std::error_code& ec){return absolute(p,ec).lexically_normal();}
inline bool is_regular_file(const path& p,std::error_code& ec){
#ifdef _WIN32
 const std::wstring wide=p.wstring(); DWORD a=GetFileAttributesW(wide.c_str()); if(a==INVALID_FILE_ATTRIBUTES){ec=std::error_code((int)GetLastError(),std::system_category());return false;} ec.clear();return !(a&FILE_ATTRIBUTE_DIRECTORY);
#else
 struct stat st; if(stat(p.string().c_str(),&st)!=0){ec=std::error_code(errno,std::generic_category());return false;}ec.clear();return S_ISREG(st.st_mode);
#endif
}
inline bool create_directories(const path& p,std::error_code& ec){
    if(p.empty()){ec.clear();return false;}
    std::string s=p.string(), cur;
    for(size_t i=0;i<s.size();++i){
        cur+=s[i];
        if(s[i]=='/'||s[i]=='\\'||i+1==s.size()){
#ifdef _WIN32
            if(cur.size()==3&&cur[1]==':') continue;
            const std::wstring wide=path(cur).wstring();
            if(!CreateDirectoryW(wide.c_str(),0)&&GetLastError()!=ERROR_ALREADY_EXISTS){ec=std::error_code((int)GetLastError(),std::system_category());return false;}
#else
            if(mkdir(cur.c_str(),0777)!=0 && errno!=EEXIST){ec=std::error_code(errno,std::generic_category());return false;}
#endif
        }
    }
    ec.clear();return true;
}
}
}
