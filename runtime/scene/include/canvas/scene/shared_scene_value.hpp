#include "canvas/foundation/internal/g5_observation.hpp"
#pragma once
#include <concepts>
#include <initializer_list>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#include <variant>

namespace canvas::scene {
// Derived Scene data only. A copy retains const storage; replacing a value never
// changes a previously published view. This is not a Semantic store or wire ID.
template<class T> class ImmutableValue final {
    std::shared_ptr<const T> value_;
  public:
    ImmutableValue() = default;
    template<class U> requires (!std::same_as<std::remove_cvref_t<U>,ImmutableValue> && std::constructible_from<T,U&&>)
    ImmutableValue(U&& v):value_(std::make_shared<T>(std::forward<U>(v))){}
    const T& get()const noexcept {static const T empty{};return value_?*value_:empty;}
    // Mutate a private local builder, then freeze a fresh allocation. No mutable
    // reference into a retained source is exposed, even for a unique owner.
    template<class F> void update(F&& f){
        T next=get();std::forward<F>(f)(next);value_=std::make_shared<const T>(std::move(next));
    }
    operator const T&()const noexcept{return get();}
    std::shared_ptr<const T> shared()const noexcept{return value_;}
    template<class U> static ImmutableValue alias(const ImmutableValue<U>& owner){
        ImmutableValue result;
        const auto& member=std::get<T>(owner.get());
        if(owner.shared())result.value_=std::shared_ptr<const T>(owner.shared(),&member);
        else result.value_=std::make_shared<const T>(member);
        return result;
    }
    bool operator==(const ImmutableValue& b)const{
        G5_OBSERVE(::canvas::foundation::internal::g5::Kind::Counter,::canvas::foundation::internal::g5::Tag::lookup,false,value_==b.value_?1ULL:2ULL,sizeof(T));
        return value_==b.value_||get()==b.get();}
    bool operator==(const T& b)const{return get()==b;}
};

template<class T> class SharedVectorView final {
    std::shared_ptr<const std::vector<T>> data_;
    const std::vector<T>& values()const noexcept {static const std::vector<T> empty;return data_?*data_:empty;}
  public:
    SharedVectorView()=default;
    explicit SharedVectorView(std::shared_ptr<const std::vector<T>> p):data_(std::move(p)){}
    const T* data()const noexcept{return values().data();}
    std::size_t size()const noexcept{return values().size();}
    bool empty()const noexcept{return values().empty();}
    const T& operator[](std::size_t i)const{return values()[i];}
    const T& front()const{return values().front();}
    const T& back()const{return values().back();}
    auto begin()const noexcept{return values().begin();}
    auto end()const noexcept{return values().end();}
    auto rbegin()const noexcept{return values().rbegin();}
    auto rend()const noexcept{return values().rend();}
    operator std::span<const T>()const noexcept{return {data(),size()};}
};

// Contiguous metadata for existing full/patch compilers. Sharing a snapshot is
// O(1); its first write clones only this table. Heavy immutable payloads remain
// shared. No writable iterator is returned from a published SharedVectorView.
template<class T> class SharedVector final {
    std::shared_ptr<std::vector<T>> data_;
    std::size_t copied_=0;
    const std::vector<T>& values()const noexcept{static const std::vector<T> empty;return data_?*data_:empty;}
    std::vector<T>& write(){
        if(!data_)data_=std::make_shared<std::vector<T>>();
        else if(data_.use_count()!=1){auto next=std::make_shared<std::vector<T>>(*data_);copied_+=data_->size();data_=std::move(next);}
        return *data_;
    }
  public:
    using value_type=T;
    using iterator=typename std::vector<T>::iterator;
    using const_iterator=typename std::vector<T>::const_iterator;
    SharedVector()=default;
    SharedVector(std::vector<T> v):data_(std::make_shared<std::vector<T>>(std::move(v))){}
    SharedVector(std::initializer_list<T> v):SharedVector(std::vector<T>(v)){}
    SharedVector(const SharedVector& v):data_(v.data_){}
    SharedVector(SharedVector&&)noexcept=default;
    SharedVector& operator=(const SharedVector& v){if(this!=&v){data_=v.data_;copied_=0;}return *this;}
    SharedVector& operator=(SharedVector&&)noexcept=default;
    SharedVectorView<T> read()const noexcept{return SharedVectorView<T>(data_);}
    const void* identity()const noexcept{return data_.get();}
    std::shared_ptr<const std::vector<T>> storage()const noexcept{return data_;}
    std::size_t copiedElements()const noexcept{return copied_;}
    std::size_t size()const noexcept{return values().size();}
    std::size_t capacity()const noexcept{return values().capacity();}
    bool empty()const noexcept{return values().empty();}
    const T* data()const noexcept{return values().data();}
    T* data(){return write().data();}
    const T& operator[](std::size_t i)const{return values()[i];}
    T& operator[](std::size_t i){return write()[i];}
    const T& front()const{return values().front();}
    T& front(){return write().front();}
    const T& back()const{return values().back();}
    T& back(){return write().back();}
    auto begin()const noexcept{return values().begin();}
    auto end()const noexcept{return values().end();}
    auto rbegin()const noexcept{return values().rbegin();}
    auto rend()const noexcept{return values().rend();}
    auto begin(){return write().begin();}
    auto end(){return write().end();}
    void reserve(std::size_t n){write().reserve(n);}
    void resize(std::size_t n){write().resize(n);}
    void clear()noexcept{data_.reset();copied_=0;}
    void swap(SharedVector& b)noexcept{data_.swap(b.data_);std::swap(copied_,b.copied_);}
    template<class It>void assign(It a,It b){auto next=std::make_shared<std::vector<T>>(a,b);copied_=next->size();data_=std::move(next);}
    operator const std::vector<T>&()const noexcept{return values();}
    void push_back(T v){write().push_back(std::move(v));}
    template<class... Args>T& emplace_back(Args&&... a){return write().emplace_back(std::forward<Args>(a)...);}
    auto erase(const_iterator a,const_iterator b){const auto first=a-values().begin(),last=b-values().begin();auto& v=write();return v.erase(v.begin()+first,v.begin()+last);}
    auto erase(const_iterator a){return erase(a,a+1);}
    bool operator==(const SharedVector& b)const{return data_==b.data_||values()==b.values();}
};
}
