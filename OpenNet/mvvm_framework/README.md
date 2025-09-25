# MVVM for C++/WinRT #
The [MVVM Design Pattern](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93viewmodel) is used quite
commonly in XAML-based UI applications. This library, known as **mvvm-winrt** provides a very lightweight set
of classes and macros to simplify the implementation of MVVM classes.

The **mvvm-winrt** library is implemented as an all-header template library that uses
[static (compile-time) polymorphism](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern#Static_polymorphism)
for greatest efficiency, especially with optimized compilation.

## View Model Classes ##

### ```mvvm::notify_property_changed<Derived>``` ###
```cpp
#include <mvvm/notify_property_changed.h>
```

#### Declaration ####
```cpp
template <typename Derived>
struct __declspec(empty_bases) notify_property_changed
```

#### Template Type Parameters
```Derived``` 
: The most-derived class; the one that you're implementing.

#### Description

This file implements the ```notify_property_changed<Derived>``` class template, which implements a base
implementation of ```INotifyPropertyChanged```. Although that interface only declares 1 member (the
```PropertyChanged``` event), this class also provides protected methods to greatly simplify property
setters and getters for most common scenarios:

- Setter implementations that return a ```bool``` indicating whether or not the property changed.
: See the ```set_property``` overloads that return a ```bool``` value:
```cpp
template <typename Value>    // On each of these

bool set_property(Value& valueField, Value const& newValue);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue);
bool set_property(Value& valueField, Value const& newValue, std::wstring_view const& propertyName);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName);
bool set_property(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames);
```

- Setter implementations that return the previous value of the backing field.
See the ```set_property*``` overloads that take a reference parameter to ```oldValue```:
```cpp
template <typename Value>    // On each of these

bool set_property(Value& valueField, Value const& newValue, Value& oldValue);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare_no_notify(Value& valueField, Value const& newValue, Value& oldValue);
```

- Setter implementations that always raise the ```PropertyChanged``` event, regardless of the value changing:
: See the ```set_property_no_compare*``` methods:
```cpp
template <typename Value>    // On each of these

void set_property_no_compare(Value& valueField, Value const& newValue, std::wstring_view const& propertyName);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName);
void set_property_no_compare(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare_no_notify(Value& valueField, Value const& newValue);
void set_property_no_compare_no_notify(Value& valueField, Value const& newValue, Value& oldValue);
```

- Setter implementations that **never** raise the ```PropertyChanged``` event:
```cpp
template <typename Value>    // On each of these

bool set_property(Value& valueField, Value const& newValue);
bool set_property(Value& valueField, Value const& newValue, Value& oldValue);
void set_property_no_compare_no_notify(Value& valueField, Value const& newValue);
void set_property_no_compare_no_notify(Value& valueField, Value const& newValue, Value& oldValue);
```

- Setter implementations that raise the ```PropertyChanged``` event for a single property name:
```cpp
template <typename Value>    // On each of these

bool set_property(Value& valueField, Value const& newValue, std::wstring_view const& propertyName);
bool set_property(Value& valueField, Value const& newValue, Value& Value, std::wstring_view const& propertyName);
void set_property_no_compare(Value& valueField, Value const& newValue, std::wstring_view const& propertyName);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName);
```

- Setter implementations that raise the ```PropertyChanged``` event for a list of property names:
```cpp
template <typename Value>    // On each of these

bool set_property(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames);
bool set_property(Value& valueField, Value const& newValue, Value& Value, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames);
void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames);
```

- Extensible to allow derived classes to provide their own flavor of thread synchronization.
: See the following ```view_model_base``` class for a discussion.

### ```mvvm::view_model_base<Derived>``` ###
```cpp
#include <mvvm/view_model_base.h>
```

This class adds a dependency upon a ```Dispatcher``` property in the ```Derived``` class. Note that this
class does not actually declare or implement this property. The derived class is responsible for this. See
the classes below for implementations that do that.

The ```get_property_core``` and ```set_property_core``` methods perform the logic of getting and setting
the backing variables. This base method should be called withing whatever thread synchronization context is
appropriate for the scenario of the derived class. One such derivation is the ```view_model_base``` class
template. It overrides the ```get_property_override``` and ```set_property_override``` methods to call
the ```get_property_core``` and ```set_property_core``` methods from within a ```CoreDispatcher```
context.

### ```mvvm::view_model<Derived>``` ###
```cpp
#include <mvvm/view_model.h>
```

This class adds the ```Dispatcher``` property used by its base class, ```view_model_base<Derived>```.

## View Classes ##

### ```mvvm::view<Derived, ViewModel>``` ###
```cpp
#include <mvvm/view.h>
```

This class should be a mix-in to a ```UIElement```-derived class, usually ```UserControl```. It also adds
a ```ViewModel``` property backed by a member variable.

### ```mvvm::view_sync_data_context``` ###
```cpp
#include <mvvm/view_sync_data_context.h>
```

This class should be a mix-in to a ```UIElement```-derived class, usually ```UserControl```. It also adds
a ```ViewModel``` property that synchronizes itself with the ```DataContext``` property of
a ```UIElement```-derived class, to which this class is a mix-in. Synchronizing the ```DataContext``` with
the ```ViewModel``` property is sometimes a useful feature from XAML.

## Commanding Classes ##

### ```mvvm::delegate_command``` ###
```cpp
#include <mvvm/delegate_command.h>
```

This class implements the command pattern, implementing ```ICommand``` by deferring to lambda expressions
provided to the contructor. This pattern is used less often now since ```{x:Bind}``` is able to bind
directly to a method. It may still be useful in cases where the ```CanExecute``` functionality is desired.
It's also provided here for completeness of an MVVM base library.

## Macros ##

###  ```NAME_OF``` and ```NAME_OF_NARROW``` ###
```cpp
#include <mvvm/name_of.h>
```
#### Declaration ####
```cpp
NAME_OF(typeName, propertyName)
NAME_OF_NARROW(typeName, propertyName)
```
#### Macro Parameters

```typeName``` 
: The name of the type that implements a member specified as ```propertyName```. This is not a
string value - only the plain text name of the type.

```propertyName``` 
: The name of the class/struct that implements the property. This is not a string value.

#### Description

```NAME_OF``` (and ```NAME_OF_NARROW``` for 8-bit characters) can be used to get the name of a property as a
```wstring_view``` or ```string_view``` ```constexpr```. This is commonly needed when checking which
property changed upon receiving a ```PropertyChanged``` event, and also when raising that event from a view
model. It will enforce at compile time that ```propertyName``` is a member of ```typeName```, then return
```propertyName``` as a ```constexpr wstring_view``` (or ```string_view```). There is no additional
runtime overhead to using this as opposed to hard coding string literals directly or in global variables when
optimized, because the Microsoft compiler de-duplicates string literals when O1 or O2 are enabled. The macros
work by using a ternary operator which the compiler can understand at compilation will always be false,
leaving only the ```string_view``` to ```propertyName```. Because these return a ```wstring_view``` or
```string_view```, the result can be directly compared to a ```wchar_t*``` or ```char*```, respectively
(whether ```const``` or not).

### Event macros ###

```cpp
DEFINE_EVENT(type, name)
```

```type```
: The event handler type, such as ```RoutedEventHandler```.

```name```
: The event name.

### Trivial/Common Property macros ###
```cpp
#include <mvvm/property_macros.h>

DEFINE_PROPERTY(type, name, defaulValue)
DEFINE_PROPERTY_PRIVATE_SET(type, name, defaulValue)
DEFINE_PROPERTY_PROTECTED_SET(type, name, defaulValue)
DEFINE_PROPERTY_READONLY(type, name, defaulValue)
```

```type```
: The type of the property.

```name```
: The name of the property. It is recommended to ue the ```NAME_OF``` macro described below.

```defaultValue```
: Use ```{}``` for the type's default value.

### Property macros with callback when the value changes ###
```cpp
#include <mvvm/property_macros.h>

DEFINE_PROPERTY_CALLBACK(type, name, defaulValue)
DEFINE_PROPERTY_CALLBACK_PRIVATE_SET(type, name, defaulValue)
DEFINE_PROPERTY_CALLBACK_PROTECTED_SET(type, name, defaulValue)
```

These macros declare a protected instance method with the following naming pattern and signature, where
```##name##``` is the name of the property. The class should implement
this method in its CPP file, or in the H file below the class:
```cpp
void On##name##Changed(type const& oldValue, type const& newValue);
```

### Property macros that do not raise ```PropertyChanged``` events ###
```cpp
#include <mvvm/property_macros.h>

DEFINE_PROPERTY_NO_NOTIFY(type, name, defaulValue)
DEFINE_PROPERTY_NO_NOTIFY_PRIVATE_SET(type, name, defaulValue)
DEFINE_PROPERTY_NO_NOTIFY_PROTECTED_SET(type, name, defaulValue)
```

### Property macros that raise ```PropertyChanged``` events whether or not the value changes ###
```cpp
#include <mvvm/property_macros.h>

DEFINE_PROPERTY_NO_COMPARE(type, name, defaulValue)
DEFINE_PROPERTY_NO_COMPARE_PRIVATE_SET(type, name, defaulValue)
DEFINE_PROPERTY_NO_COMPARE_PROTECTED_SET(type, name, defaulValue)
```
