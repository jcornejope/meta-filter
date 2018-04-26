#include <vector>
#include <algorithm>
#include <ostream>

struct Card
{
    Card( int id, char const* name, float cost, int version, int leader_id )
        : m_name( name )
        , m_cost( cost )
        , m_id( id )
        , m_version( version )
        , m_leader_id( leader_id )
    {
    }

    friend std::ostream& operator<<( std::ostream& os, Card const& card )
    {
        return os << "[" << card.m_id << "] " << card.m_name.c_str() << " (" << card.m_cost << ") [" << card.m_version << ", " << card.m_leader_id << "]";
    }

    std::string m_name;
    float m_cost;
    int m_id;
    int m_version;
    int m_leader_id;
};

typedef std::vector<Card*> CardList;

struct EmptyFilter
{
    bool evaluate( Card const& card ) const { return true; }
};

struct CostFilter
{
    CostFilter() : m_max_cost( std::numeric_limits<float>::max() ), m_min_cost( 0.f ) {}

    CostFilter& max_cost( float value ) { m_max_cost = value; return *this; }
    CostFilter& min_cost( float value ) { m_min_cost = value; return *this; }

    bool evaluate( Card const& card ) const { return card.m_cost > m_min_cost && card.m_cost < m_max_cost; }

    float m_max_cost;
    float m_min_cost;
};

struct VersionFilter
{
    VersionFilter() {}
    VersionFilter(std::initializer_list<int> init_list) : m_versions(init_list) {}

    VersionFilter& add_version( int version ) { m_versions.push_back(version); return *this; }

    bool evaluate( Card const& card ) const { return std::find(m_versions.begin(), m_versions.end(), card.m_version) != m_versions.end(); }

    std::vector<int> m_versions;
};

template <typename Filter = EmptyFilter, typename ...MoreFilters>
struct MetaFilter : public Filter, public MoreFilters...
{
    bool evaluate( Card const& card )
    {
        bool ret = Filter::evaluate( card );
        int dummy[sizeof...(MoreFilters)+1] = { ( ret = ret && MoreFilters::evaluate( card ), 0 )... };
        (void)dummy;
        return ret;
    }
};
