#ifndef Any_h
#define Any_h

#include <functional>
#include <memory>
#include <iostream>

namespace mc {
    
    class Any
    {
    public:
        struct AnyKeeperBase
        {
            virtual ~AnyKeeperBase() {}
            virtual const std::type_info& type() const noexcept = 0;
            virtual AnyKeeperBase * clone() const = 0;
        };
      
        
        template <class ValueType>
        struct AnyKeeper : public AnyKeeperBase
        {
            AnyKeeper(const ValueType & obj) : _storedObject(obj) {}
            AnyKeeper(ValueType && obj) : _storedObject(std::move(obj)) {}
            
            virtual const std::type_info & type() const noexcept
            {
                return typeid(ValueType);
            }
            
            virtual AnyKeeperBase * clone() const
            {
                return new AnyKeeper(_storedObject);
            }

            ValueType _storedObject;
            
            AnyKeeper & operator=(const AnyKeeper &) = delete;
        };
        
        
    public:

        //perfect fowarding constructor
        template <class ValueType,
                  typename std::enable_if<!(std::is_same<ValueType, Any&>::value || //  go away Any&, the copy constructor is right there
                                            std::is_const<ValueType>::value)>::type* = nullptr> // no const ValueType && shall pass
        explicit Any(ValueType && objectToStore) : _holder(new AnyKeeper<typename std::decay<ValueType>::type>(static_cast<ValueType&&>(objectToStore)))
        {
            static_assert(!std::is_pointer<typename std::remove_reference<ValueType>::type >::value, "pointers are not supported");
        }
        
        Any() noexcept : _holder(nullptr) {}
        Any(const Any & other) : _holder( other._holder ? other._holder->clone() : nullptr) {}
        Any(Any && other) : _holder(std::move(other._holder)) {}
        
        Any & operator=(const Any& rhs)
        {
            Any(rhs).swap(*this);
            return *this;
        }

        Any & operator=(Any&& rhs) noexcept
        {
            rhs.swap(*this);
            Any().swap(rhs); //this is to immediately destruct the contents that are in rhs and were in *this, is this needed....? they do it in boost...
            return *this;
        }

        template <class ValueType>
        Any & operator=(ValueType&& rhs)
        {
            Any(static_cast<ValueType&&>(rhs)).swap(*this);
            return *this;
        }
        
        Any & swap(Any & rhs) noexcept
        {
            std::swap(_holder, rhs._holder);
            return *this;
        }
        
        bool empty() const noexcept
        {
            return _holder != nullptr;
        }
        
        void clear() noexcept
        {
            Any().swap(*this);
        }
        
        const std::type_info& type() const noexcept
        {
            return _holder ? _holder->type() : typeid(void);
        }
        
        //just here to explain stuff
        template<typename ValueType>
        ValueType & anyCast()
        {
            return anyCast<ValueType>(const_cast<Any&>(*this));
        }
        
        std::unique_ptr<AnyKeeperBase> _holder;
        
    };
    

    class BadAnyCast : public std::exception
    {
    public:
        virtual const char * what() const noexcept
        {
            return "mc::BadAnyCast: "
                   "failed conversion using mc::AnyCast";
        }
    };
    
    template<typename ValueType>
    ValueType * anyCast(Any * operand) noexcept
    {
        return operand && operand->type() == typeid(ValueType) ?  &static_cast<typename mc::Any::AnyKeeper<typename std::remove_cv<ValueType>::type>* >(operand->_holder.get())->_storedObject : nullptr;
    }
    
    template<typename ValueType>
    inline const ValueType * anyCast(const Any * operand) noexcept
    {
        return anyCast<ValueType>(const_cast<Any *>(operand));
    }
    
    template<typename ValueType>
    ValueType anyCast(Any & operand)
    {
        using nonref = typename std::remove_reference<ValueType>::type;
        nonref * result = anyCast<nonref>(&operand);
        
        if(result == nullptr)
            throw BadAnyCast();
        
        return static_cast<ValueType>(*result);
        
        using ref_type = typename std::conditional<std::is_reference<ValueType>::value,
                                        ValueType,
                                        typename std::add_lvalue_reference<ValueType>::type>::type;
                                
        return static_cast<ref_type>(*result);
    }
    
    template<typename ValueType>
    inline ValueType anyCast(const Any & operand)
    {
        using nonref = typename std::remove_reference<ValueType>::type;
        return anyCast<const nonref &>(const_cast<Any &>(operand));
    }


    template<typename ValueType>
    inline ValueType anyCast(Any && operand)
    {
        static_assert(
            std::is_rvalue_reference<ValueType&&>::value /*true if ValueType is rvalue or just a value*/
            || std::is_const< typename std::remove_reference<ValueType>::type >::value,
            "anyCast shall not be used for getting nonconst references to temporary objects" 
        );
        return anyCast<ValueType>(operand);
    }
}


#endif