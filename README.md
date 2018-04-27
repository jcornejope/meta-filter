# The Meta-filter: Using variadic templates for filtering data. 

### Searches and filters 

During the development of most games we encountered several situations in which we needed to query the different systems for information. Things like _'what units are near this position?'_, _'what units on my team are alive and in range?'_ or _'what cards from this specific leader do I own?'_... 

For such queries we usually implement the functions that searches for the desired information and then filters the results (or avoid searching for some data altogether) to match the criteria the user of the system wants to obtain. This is a common practice and its done in a regular basis in game development. 

### A new feature is needed 

When we started developing the queries and filters for a card game I'd work on we didn't have a clear design for which filters would be needed or how many so when we started thinking on the different possible implementations we had clear that flexibility and extendability would be an important factor for the implementation of the system. We could follow other implementations in the codebase where we have a big function that receives lots of configuration data for every single option and then it does checks on the existence of data to apply the filter or not. Something like this: 

```c++
struct QueryData
{
    // Constructors, etc... 

    std::vector<long> m_players;
    std::vector<long> m_teams;
    Vector3 m_position;
    float m_range;
    bool m_exclude_dead;

    // Many more members... 
};

void get_information( QueryData const& data, std::vector<Info*>& out_infos );
```

Doing so we could have a system that just works and we could move to the next task. But if you think about this kind of implementation you soon realize that it isn't really a system that is flexible or easy to extend and maintain so it actually fails to fulfil the needs that we have. 

### Looking for new options 

A different option for the implementation is to rather than create a mega filter that handles all the cases and needs to check for the existence, lack or a value different than certain default value (which fixes the precedence of the filters and is tedious to expand and maintain) is to create micro-filters that only focuses on single purposes and which we only instantiate if it needs to be applied, this will also allow us to change the precedence of the filters based on the client needs, will be easier to maintain and will be easily expanded by creating new micro-filters. 

This micro filter approach seems like a good idea from the maintenance, modularity and flexibility point of view, in the other hand it will add some function calls so it may have some performance overhead but overall the pros seems to overcome the cons as this new system will not be used in critical sections of the code, so let's think how it can be implemented. 

Our first option could be to declare a common `FilterInterface` with a virtual function that evaluates the card and returns whether or not it accepts it. All the filters that we implement should inherit from the interface and override the `evaluate` function. Then the database function that queries the data will receive a list of these filters and will apply them in order. 

```c++
struct FilterInterface 
{ 
	bool evaluate( Card const& card ) const = 0; 
}; 

struct Filter1 : public FilterInterface 
{ 
	bool evaluate( Card const& card ) const { /* do stuff and return; */ return true; } 
}; 

typedef std::vector<Card*> CardList;
typedef std::vector<FilterInterface*> FilterList; 

// Returns a subset of cards that matches the criteria defined on the filter parameter. 
size_t get_card_list( FilterList const& filters, CardList& out_cards ) const; 
```

This approach will increase the flexibility, modularity and extendability so it will cover our main goals but it will also add the function calls overhead, the virtualization overhead and the fact that it will make impossible for the compiler to inline the function calls (remember that inlining is a step that is performed at compile time but at that time we don't know what version of the virtual function we need to use, that's something we know at run time), and more important it will also have a decrease on the usability as it will require many lines of code and will become annoying to use: 

```c++
FilterList filter_list; 
filter_list.reserve( 3 ); 

Filter1 f1; 
f1.set_option1( value ); 
f1.set_option2( value2 ); 
... 
filter_list.push_back( f1 ); 

Filter2 f2; 
f2.set_option1( value ); 
f2.set_option2( value2 ); 
... 
filter_list.push_back( f2 ); 

Filter3 f3; 
f3.set_option1( value ); 
f3.set_option2( value2 ); 
... 
filter_list.push_back( f3 ); 

CardList card_list; 
get_card_list( filter_list, card_list ); 
```
So, can we keep the benefits of this implementation but do better? of course we can. One thing that we typically do to optimize virtual function calls is to switch from virtual inheritance to templates so it seems logical to try this approach here. This change will implicitly create an interface that the filter must follow (or it won't compile) but will remove the virtualization and will allow the inlining (if possible) with the performance benefits it carries. 

For this we will need to adapt the query function in the database with a signature like this one: 

```c++
// Returns a subset of cards that matches the criteria defined on the filter parameter. 
template<class Filter> 
uint get_card_list( Filter& filter, CardList& out_cards ) const; 
``` 

and an implementation like this: 

```c++
template<class Filter> 
uint CardDatabase::get_card_list( Filter& filter, CardList& out_cards ) const 
{ 
	out_cards.clear(); 
	out_cards.reserve( m_cards.size() ); 

	for( auto const& card : m_cards ) 
		if( filter.evaluate( card ) ) 
			out_cards.push_back( card ); 

	return out_cards.size(); 
} 
```

Then each filter will be implemented almost as before but no inheritance needed 

```c++
struct Filter1 
{ 
	bool evaluate( Card const& card ) const { /* do stuff with card and return */ return true; } 
}; 
```

### Building up the templated filter 

This change in the database should be enough for using a single filter with the query but how can we make it so we can use multiple filters at the same time? Any number of filters in any order. We could try using **variadic templates**. A variadic template class can be instantiated with any number of template arguments so we can create a single `MetaFilter` that encapsulates all the filters we need. 

```c++
template <typename ...Filters> 
``` 

Now we can have multiple `Filters` in our template, we use the ellipsis `...` to denotate the template parameter pack. We can create that new `MetaFilter` which encapsulates all those single `Filters` either by using composition or inheritance. In this case inheritance will give us a plus on usability as it will be easier to fill data into the multiple filters and will require less code, so let's go for it: 

```c++
template <typename ...Filters> 
struct MetaFilter : public Filters... 
{ 
	bool evaluate( Card const& card ) const {} 
}; 
```

Note that the template parameter pack will be expanded where a comma separated list is allowed and the _base-clause_ from a _class declaration_ is no exception. The ellipsis will appear on the left to declare the parameter pack whereas it will appear on the right in the template pack expansion. For more information visit [parameter_pack](http://en.cppreference.com/w/cpp/language/parameter_pack). 

We need to implement the `evaluate` function next; we need to iterate through the different `Filters` types and call to its `evaluate` function. In order for us to do it, we need to unpack all the template parameters. There's two main ways of doing this: 

#### Recursive expansion 

The first way to unpack template parameters is by recursion, if you are not familiar with templates you are probably frowning your eyebrows and your brain is probably screaming: _'Recursion is bad! it will be super slow and expensive! don't do it!!'_ but when we talk about recursion in metaprogramming you need to take into account that we are operating at compile time not at run time so the compiler will unroll the recursive calls and produce much more optimal code (on unoptimized builds you will still be able to debug the code with the recursive calls so you can follow the code step by step) so don't worry about it. 

Ok, so how can we recursively expand the template parameter pack? What we need is to create two function templates, one that will receive as argument a single type from the parameter pack (`Filter`) and another argument with the rest of the types (`...MoreFilters`), every time we call this function the parameter pack will be expanded to the first parameter and the rest of types will go as a new parameter pack with one less type. The second function will need only the single type argument (`Filter`) as this will be called when we have no more types to expand: 

```c++ 
template <typename ...Filters> 
struct MetaFilter : public Filters... 
{ 
	template <typename Filter> 
	bool evaluateFilter( Card const& card, Filter const& filter ) const 
	{ 
		return filter.evaluate( card ); 
	} 

	template <typename Filter, typename ...MoreFilters> 
	bool evaluateFilter( Card const& card, Filter const& filter, MoreFilters const& ...more ) const 
	{ 
		if( filter.evaluate( card ) ) 
			return evaluateFilter( card, more... ); // recursive call 

		return false; 
	} 

	bool evaluate( Card const& card ) const 
	{ 
		return evaluateFilter( card, *static_cast<Filters const*>(this)... ); 
	} 
}; 
```

Suppose you have a template parameter pack with the types ´<bool, int, float>´, then the calls will expand the parameters as follows: 

```c++
evaluateFilter( Card const&, <bool, int, float> const&); 

bool evaluateFilter( Card const&, bool const& , <int, float> const&) const; 
bool evaluateFilter( Card const&, int const&  , <float> const&) const;
bool evaluateFilter( Card const&, float const&) const;
``` 

This way of expanding parameter packs is especially useful when we want to execute different code for the last parameter type but as you can see it needs some extra functions to do the trick. Let's evaluate the second way to expanding the parameter packs. 

#### Using list initialization for expansion 

The second way seizes the fact that the template parameter pack will be expanded where a comma separated list is allowed so we can use the list initialization which not only lets you expand the template parameter pack but also guarantees sequencing [list_initialization](http://en.cppreference.com/w/cpp/language/list_initialization). This means that it can be used to call a function or perform an operation on each type in the parameter pack in order: 

```c++
template <typename ...Filters> 
struct MetaFilter : public Filters... 
{ 
	bool evaluate( Card const& card ) 
	{ 
		bool ret = true; 
		int dummy[sizeof...(Filters)] = { ( ret = ret && Filters::evaluate( card ) ,0 )... }; 
		(void)dummy; 
		return ret; 
	} 
}; 
```

Neat! isn't it? :). Let's take a look to how it works. We are creating a `dummy` array just to be able to use the _braced-init-list_. Note that we will not be using the array at all so we add `(void)dummy` (in line 8) to silence a warning of unused variable (you may not need this depending on the compiler and the warnings configuration you have set up). 

Please note that `sizeof...( Filters )` is not really needed (for now) but it is useful for illustrative purposes. `sizeof...` will return the number of types on the template parameter pack, so we will declare the `dummy` array with the same length of the parameter pack. 

Everything seems fine, isn't it?... ... not really, if you remember we stated before: "A variadic template class can be instantiated with **any** number of template arguments", note the emphasis on that any, that means that we can also receive 0 types; it will result on a declaration of `dummy` as follows: 

```c++
int dummy[0]; 
```

This is wrong as we cannot allocate an array of constant size 0, it doesn't make sense and actually won't compile. We could live with it an assume that we don't support that case. If that's what we want we can add a `static_assert` to the code in the evaluate function to give a more descriptive error to the users: 

```c++
static_assert( sizeof...(Filters) > 0, "The MetaFilter doesn't supports empty parameter pack :(" ); 
```
 
That will work but it is not hard to actually support it and it may be useful for the users as we could consider an empty `MetaFilter` as a filter that just accepts all the cards so we don't need to override the `get_card_list` function. 

```c++
MetaFilter<> empty_filter; 
get_card_list(empty_filter, card_list); 
``` 

That will return all the cards but we would need to do a bit of extra work to achieve that so let's go back to that later. 

The only thing we need to fix this problem is just add a `+ 1` on the size of the array so it is always bigger than 0 :) 

```c++ 
int dummy[sizeof...(Filters) + 1]; 
```

Let's now focus on the _braced-init-list_: 

```c++
int dummy[sizeof...(Filters) + 1] = { ( ret = ret && Filters::evaluate( card ), 0 )... }; 
``` 

In order to expand the parameter pack we use the _braced-init-list_ as we said before, we just call the inherited function from each type in the parameter pack; as we are inheriting multiple versions of that same function we need to call to different `evaluate` funtion each time. One interesting caviat with this approach is that it won't compile in _VisualStudio 2015_ (it does compile and work in other major compilers like GCC 4.7 onwards, clang 3.8 or VS2017), it will throw and error stating that there's not static function evaluate (it doesn't get the inheritance correctly I guess). We can workaround that in that compiler with 

```c++
int dummy[sizeof...(Filters) + 1] = { ( ret = ret && static_cast<Filters*>(this)->evaluate( card ), 0 )... }; 
``` 

You may be now wondering what's about that parenthesis and that `, 0`, why do we need that for?. The answer is that in our case we actually don't need that **but** again is useful for illustrative purposes. As we know the return type of the function we could use the _braced-init-list_ to build an `std::initializer_list`: 

```c++
std::initializer_list<bool> dummy = { ret = ret && Filters::evaluate( card )... }; 
``` 

or even 

```c++
auto dummy = { true, ret = ret && Filters::evaluate( card )... }; 
// The "true" value here is to support the zero types case on the parameter pack and the auto being able to determine the type (std::initializer_list<bool>). 
```

So instead of creating an array with all the issues of the size that we saw we can create an `std::initializer_list` instead (that by default supports the zero elements case). As I said for our purposes it will suffice but it has some restrictions, the most important one is the fact that you cannot call functions that returns `void` as `std::initializer_list<void>` is not allowed and won't compile. 

Instead we use the array and the _comma operator_ (that's why we need the parenthesis and the `, 0` for), in a nutshell the _comma operator_ works in the way `exp1, exp2` where `exp1` is evaluated and its result is then "discarded" but its side effects will remain and be completed before evaluating `exp2`, the result from evaluating `exp2` will then be used. If you're not familiar with the _comma operator_ and want to know more you can refer to [comma operator](http://en.cppreference.com/w/cpp/language/operator_other) for more information. This means that the function call is now a side effect and the result is just an int (`0`); that's why `dummy` is of type `int[]`.

Finally notice that `ret` basically stores the result of each function call to be returned later but also shot-circuits the calls to evaluate, so if one of the filters fails the rest will just be not called as `ret` will be already `false`. 

At last we have a filter that will work as we intended... almost. We still want to support the case for zero parameter types. For that we can define a filter that always returns true: 

```c++ 
struct EmptyFilter 
{ 
	bool evaluate( Card const& ) const { return true; } // No parameter name to avoid unused variable warning.
}; 
```

and then use it as default template parameter type. For that we need to add a new type that we default to that `EmptyFilter` and then call it.: 

```c++ 
template <typename Filter = EmptyFilter, typename ...MoreFilters> 
struct MetaFilter : public Filter, public MoreFilters... 
{ 
	bool evaluate( Card const& card ) 
	{ 
		bool ret = Filter::evaluate( card ); 
		int dummy[sizeof...(MoreFilters) + 1] = { ( ret = ret && MoreFilters::evaluate( card ), 0 )... }; 
		(void)dummy; 
		return ret; 
	} 
}; 
```

Now we finally have the filters working as we want it and the users can generate code like this to be used: 

```c++ 
MetaFilter<LeaderFilter, VersionFilter, ManaFilter> filter; 
filter.addLeader( "MegaLeader1" ).addLeader( 3 ); 
filter.addVersion( "DLC1" ); 
filter.addMaxCost( 30 ).addMinCost( 5 ).setInclusiveMode( true ); 

CardList card_list; 
get_card_list(filter, card_list); 
``` 

Note that the declaration of these filters returns a reference to itself so it can chain calls on the same type: 

```c++ 
struct LeaderFilter 
{ 
	... 
	LeaderFilter& add_leader( long id ) { m_leaders.push_back( id ); return *this; } 

	bool evaluate( CardList const& card ) const; 
	... 
}; 
```

### Conclusion 

We have created a system that is easier to maintain as each filter is focused on one task, it is easier to understand and to expand if needed, it is modular and more flexible as we can mix and match all the filters we want with the precedence we want. 

This may not be the best solution for all the cases, if the performance is a concern in your case you may consider other options which will be more optimal as will not beat on performance other implementations, although this same solution may be optimized (maybe passing all the cards to `evaluate` rather than just one will probably give a boost as we don't need to do so many function calls but it will require handling the data structures on the filters). 

I would say that the more important takeover from this article is that developing something new in an extensive code base doesn't mean, or justify, that we should just copy-paste from other system that just works, we should think about the possibilities, evaluate our restrictions and necessities and then decide what fits best, if in the way you also learn something new that's even better!. Don't let the production times drive your code decisions!. 

Overall this implementation gives us a lot of advantages, including our most important goal: scalability; in our case performance wasn't an issue so it fit perfectly and let me tell you one last thing: 

>Damn! It was fun to build! :) 