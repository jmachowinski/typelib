#include "typelib.hh"

using namespace Typelib;

/**********
 *  Define typelib_(to|from)_ruby
 */

/* This visitor takes a Value class and a field name,
 * and returns the VALUE object which corresponds to
 * the field, or returns nil
 */
class RubyGetter : public ValueVisitor
{
    VALUE m_value;
    VALUE m_registry;

    virtual bool visit_ (int8_t  & value) { m_value = CHR2FIX(value); return false; }
    virtual bool visit_ (uint8_t & value) { m_value = CHR2FIX(value); return false; }
    virtual bool visit_ (int16_t & value) { m_value = INT2NUM(value); return false; }
    virtual bool visit_ (uint16_t& value) { m_value = INT2NUM(value); return false; }
    virtual bool visit_ (int32_t & value) { m_value = INT2NUM(value); return false; }
    virtual bool visit_ (uint32_t& value) { m_value = INT2NUM(value); return false; }
    virtual bool visit_ (int64_t & value) { m_value = LL2NUM(value);  return false; }
    virtual bool visit_ (uint64_t& value) { m_value = ULL2NUM(value); return false; }
    virtual bool visit_ (float   & value) { m_value = rb_float_new(value); return false; }
    virtual bool visit_ (double  & value) { m_value = rb_float_new(value); return false; }

    virtual bool visit_(Value const& v, Pointer const& p)
    {
        m_value = cxx2rb::value_wrap(v, m_registry, cType);
        return false;
    }
    virtual bool visit_(Value const& v, Array const& a) 
    {
        m_value = cxx2rb::value_wrap(v, m_registry, cArray);
        return false;
    }
    virtual bool visit_(Value const& v, Compound const& c)
    { 
        m_value = cxx2rb::value_wrap(v, m_registry, cCompound);
        return false; 
    }
    virtual bool visit_(Enum::integral_type& v, Enum const& e)   
    { 
        m_value = cxx2rb::enum_symbol(v, e);
        return false;
    }
    
public:
    RubyGetter()
        : ValueVisitor(false) {}

    VALUE apply(Typelib::Value value, VALUE registry)
    {
        m_registry = registry;
        m_value = Qnil;
        ValueVisitor::apply(value);
        return m_value;
    }
};

class RubySetter : public ValueVisitor
{
    VALUE m_value;

    virtual bool visit_ (int8_t  & value) { value = NUM2CHR(m_value); return false; }
    virtual bool visit_ (uint8_t & value) { value = NUM2CHR(m_value); return false; }
    virtual bool visit_ (int16_t & value) { value = NUM2LONG(m_value); return false; }
    virtual bool visit_ (uint16_t& value) { value = NUM2ULONG(m_value); return false; }
    virtual bool visit_ (int32_t & value) { value = NUM2LONG(m_value); return false; }
    virtual bool visit_ (uint32_t& value) { value = NUM2ULONG(m_value); return false; }
    virtual bool visit_ (int64_t & value) { value = NUM2LL(m_value);  return false; }
    virtual bool visit_ (uint64_t& value) { value = NUM2LL(m_value); return false; }
    virtual bool visit_ (float   & value) { value = NUM2DBL(m_value); return false; }
    virtual bool visit_ (double  & value) { value = NUM2DBL(m_value); return false; }

    virtual bool visit_(Value const& v, Array const& a)
    { 
        if (a.getIndirection().getName() == "/char")
        {
            char*  value = StringValuePtr(m_value);
            size_t length = strlen(value);
            if (length < a.getDimension())
            {
                memcpy(v.getData(), value, length + 1);
                return false;
            }
        }
        throw UnsupportedType(v.getType()); 
    }
    virtual bool visit_(Value const& v, Compound const& c)
    { 
        throw UnsupportedType(v.getType()); 
    }
    virtual bool visit_(Enum::integral_type& v, Enum const& e)
    { 
        v = rb2cxx::enum_value(m_value, e);
        return false;
    }
    
public:
    RubySetter()
        : ValueVisitor(false) {}

    VALUE apply(Value value, VALUE new_value)
    {
        m_value = new_value;
        ValueVisitor::apply(value); 
        return new_value;
    }
};

/*
 * Convertion function between Ruby and Typelib
 */

/* Converts a Typelib::Value to Ruby's VALUE */
VALUE typelib_to_ruby(Value v, VALUE registry)
{ 
    if (! v.getData())
        return Qnil;

    RubyGetter getter;
    return getter.apply(v, registry);
}

/* Converts a given field in +value+ */
VALUE typelib_to_ruby(Value value, VALUE name, VALUE registry)
{
    if (! value.getData())
        return Qnil;

    try { 
        Value field_value = value_get_field(value, StringValuePtr(name));
        return typelib_to_ruby(field_value, registry);
    } 
    catch(FieldNotFound)   {} 
    rb_raise(rb_eArgError, "no field '%s'", StringValuePtr(name));
}

/* Tries to initialize +value+ to +new_value+ using the type in +value+ */
VALUE typelib_from_ruby(Value value, VALUE new_value)
{
    std::string type_name;
    try {
        RubySetter setter;
        return setter.apply(value, new_value);
    } catch(UnsupportedType e) { type_name = e.type.getName(); }
    rb_raise(rb_eTypeError, "cannot convert to '%s'", type_name.c_str());
}
