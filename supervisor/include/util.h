// -*- mode: c++; eval: (c-set-style "stroustrup"); eval: (c-set-style "archon-cc-mode"); -*-

#ifndef ARCHON_UTIL_H_
#define ARCHON_UTIL_H_

#include <utility>

namespace archon {

/*! 
  Move-only object wrapping a callable entity we wish to run upon exiting scope.
  Can only be called 1 time, then it automatically disarms. This was inspired by
  Google Fuchsia's FBL auto_call.h interface.
*/

template <typename T>
class OnExitScopeCall {
public:
    constexpr explicit OnExitScopeCall(T c) : scoped_call_(std::move(c)) {}
    ~OnExitScopeCall()  { early_call(); }

    OnExitScopeCall(OnExitScopeCall&& oesc) :
	scoped_call_(std::move(oesc.scope_call_)), armed_(oesc.armed_) {
	oesc.disarm();
    }
    
    OnExitScopeCall& operator = (OnExitScopeCall&& oesc) {
	early_call();
	scoped_call_ = std::move(oesc.scope_call_);
	armed_ = oesc.armed_;
	oesc.disarm();
	return *this;
    }

    OnExitScopeCall(const OnExitScopeCall&) = delete;
    OnExitScopeCall& operator = (const OnExitScopeCall&) = delete;

    //! explicitly calling it disarms it. Disarm first to prevent recursion.
    void early_call() {
	bool armed = armed_;
	disarm();
	if (armed) {
	    (scoped_call_)();
	}
    }
    
    void disarm() { armed_ = false; }

private:
    T scoped_call_;
    bool armed_ = true;
};

//! Convenience wrapper
template <typename T>
inline OnExitScopeCall<T> MakeOnExitScopeCall(T c) {
    return OnExitScopeCall<T>(std::move(c));
}

} // namespace archon


#endif // ARCHON_UTIL_H_
