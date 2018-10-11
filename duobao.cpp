#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/transaction.hpp>
#include <eosiolib/singleton.hpp>
#include <boost/random.hpp>
#include <ctime>
#include <vector>
#include <map>


using namespace eosio;
using std::string;
using std::hash;

class duobao: public eosio::contract{

public:


   
	duobao(account_name self)
	:contract(self),
	bets(_self, _self),
	winners(_self, _self)
    {
    }

    using contract::contract;
    
    static constexpr int64_t unit_count = 10;
    static constexpr int64_t min_count = 1 * unit_count;
    static constexpr int64_t max_count = 20 * unit_count;
    static constexpr float win_ratio = 0.9;


    uint64_t random_generate(uint64_t range) {

         checksum256 result;
         auto mixedBlock = tapos_block_prefix() * tapos_block_num();
         const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
         sha256( (char *)mixedChar, sizeof(mixedChar), &result);
         const char *p64 = reinterpret_cast<const char *>(&result);

         int64_t sum = 0;
         for(int i = 0; i<6; i++) {
            sum += (int64_t)p64[i];
         }
         return (abs(sum) % (range));
    }

	void buy(account_name from, account_name to, asset quantity, string memo) {
    	if ((from == _self) || (to != _self)) {
        	return;
    	}
    	eosio_assert(quantity.symbol == CORE_SYMBOL, "Must by paid by EOS");
    	eosio_assert(quantity.amount % min_count == 0, "Bet Must Be divided by 1 EOS");
        eosio_assert(quantity.amount >= min_count, "Bet Must be greater than 1 EOS");


        auto new_offer_itr = bets.emplace(_self, [&](auto& betting){
            betting.id         = bets.available_primary_key();
            betting.bet        = quantity;
            betting.owner      = from;
            betting.memo     = memo;
        });

        uint64_t total = query_total();

        if( total >= max_count ){
    		doOpen(total);
    	}
    }

    uint64_t query_total(){

        uint64_t sum = 0;
    	for( const auto& item : bets ) {
    		sum += item.bet.amount;
    	}
    	return sum;

    }


    void doOpen(uint64_t total) {

    	uint64_t towin = random_generate(total / unit_count) * unit_count, inc = 0;

    	for( const auto& item : bets ) {
    		// the winner
    		if( (towin >= inc) && (towin < (inc + item.bet.amount)) ) {

    			auto new_offer_itr = winners.emplace(_self, [&](auto& winner){
    			 	winner.id         = winners.available_primary_key();
            	    winner.owner      = item.owner;
           			winner.bet        = item.bet;
           			winner.total      = total;
           			winner.res        = towin;
        		});

    			action(
            		permission_level{_self, N(active)},
            		N(eosio.token), 
            		N(transfer),
            		std::make_tuple(_self, item.owner, asset(total * win_ratio), item.memo)
         		).send();

         		break;
    		}
    		inc += item.bet.amount;
		}

		for(auto itr = bets.begin(); itr != bets.end(); ){
        	itr = bets.erase(itr);
    	}
    }
    // @abi action
    [[eosio::action]]
    void open( account_name user ) {
    	require_auth( user );
    	doOpen(query_total() );
    }



private:
      //@abi table offer i64
      struct [[eosio::table]] betting {
         uint64_t          id;
         account_name      owner;
         asset             bet;
         string          memo;

         uint64_t primary_key()const { return id; }

         uint64_t by_bet()const { return (uint64_t)bet.amount; }

         EOSLIB_SERIALIZE( betting, (id)(owner)(bet)(memo) )
      };

	  //@abi table offer i64
      struct [[eosio::table]] winner {
        
         uint64_t     id;
         account_name owner;
         asset        bet;
         uint64_t     total; 
         uint64_t     res;
       
         uint64_t primary_key()const { return id; }

         EOSLIB_SERIALIZE( winner, (id)(owner)(bet)(total)(res) )
      };


      typedef eosio::multi_index< N(betting), betting > bet_index;
      typedef eosio::multi_index< N(winner), winner> account_index;

      bet_index bets;
      account_index winners;
};

extern "C" {
    void apply( uint64_t receiver, uint64_t code, uint64_t action ) {

        duobao thiscontract(receiver);

        if((code == N(eosio.token)) && (action == N(transfer))) {
            execute_action(&thiscontract, &duobao::buy);
            return;
        }

        if (code != receiver) return;

        switch (action) {
            EOSIO_API(duobao, (open))
        };
        eosio_exit(0);
    }
}
