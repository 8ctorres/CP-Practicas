-module(notifier).

%API functions
-export([start/0, register/1, send/2, stop/1]).
%Aux function
-export([loop/1]).

%API

start() ->
    spawn(?MODULE, loop, [[]]).

register(N) ->
    N ! {register, self()},
    ok.

send(N, Msg) ->
    N ! {broadcast, Msg},
    Msg.

stop(N) ->
    N ! stop.

%Internal functions

loop(Registered) ->
    receive
        {broadcast, Msg} ->
            broadcast(Registered, Msg),
            loop(Registered);
        {register, Pid} ->
            loop([Pid|Registered]);
        stop ->
            stop
    end.

broadcast(List, Msg) ->
    case List of
        [] ->
            ok;
        [Pid|Tail] ->
            Pid ! Msg,
            broadcast(Tail, Msg)
    end.
