-module(store).

%API functions
-export([start/0, store/2, get/2]).
%Aux functions
-export([loop/1]).

%% API

start() ->
    spawn(?MODULE, loop, [[]]).

store(S,P) ->
    S ! {store, P},
    P.

get(S,F) ->
    S ! {get, F, self()},
    receive
        {ok, P} ->
            {ok, P};
        {error, no_product} ->
            {error, no_product}
    end.

%% Internal functions

loop(Storage) ->
    receive
        {store, P} ->
            loop([P|Storage]);
        {get, F, From} ->
            Prod = get_aux(F, Storage),
            From ! Prod,
            case Prod of
                {error, no_product} ->
                    loop(Storage);
                {ok, P} ->
                    loop([X || X <- Storage, X/= P])
             end;
        stop ->
            stop
    end.

get_aux(Predicate, Storage) ->
    case Storage of
        [] ->
            {error, no_product};
        [H|T] ->
            case H of
                Predicate -> {ok, H};
                _ ->
                    get_aux(Predicate, T)
            end
    end.
