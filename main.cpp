#include <iostream>
#include "metafilter.h"

template<typename Filter>
size_t get_cards( CardList const& cards, Filter filter, CardList& out_cards )
{
    out_cards.clear();
    out_cards.reserve( cards.size() );

    for( auto const& card : cards )
        if( filter.evaluate( *card ) )
            out_cards.push_back( card );

    return out_cards.size();
}

int main( int argc, char* argv[] )
{
    // Init some test data
    CardList cards =
    {
        //         ID   NAME    COST    VER.   LEADER
        new Card{   0, "Card1", 30.f,    1,      0 },
        new Card{   1, "Card2", 10.f,    1,      0 },
        new Card{   2, "Card3", 12.5f,   1,      1 },
        new Card{   3, "Card4", 100.f,   1,      1 },
        new Card{   4, "Card5", 45.f,    2,      1 }
    };

    // Create filters
    MetaFilter<CostFilter, VersionFilter> filter;
    filter.add_version( 1 ).add_version( 3 );
    filter.max_cost( 50.f );

    // Filter cards and get the out cards
    CardList out_cards;
    get_cards( cards, filter, out_cards );

    // Print the cards
    std::cout << "These are the filtered cards:" << std::endl;
    for( auto const card : out_cards )
        std::cout << *card << std::endl;

    getchar();

    // Tidy up before leaving
    out_cards.clear();
    for( auto const card : cards )
        delete card;
    cards.clear();

    return 0;
}