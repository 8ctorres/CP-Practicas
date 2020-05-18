-module(comp).

-export([comp/1, comp_proc/2, comp_proc/3, decomp/1, decomp/2]).
-export([comp_workers_loop/3]).

-define(DEFAULT_CHUNK_SIZE, 1024*1024).
-define(DEFAULT_PROCS, 3).

%%% File Compression

comp(File) ->
    comp_proc(File,?DEFAULT_CHUNK_SIZE,?DEFAULT_PROCS).

comp_proc(File,Procs) ->
    comp_proc(File,?DEFAULT_CHUNK_SIZE,Procs).

comp_proc(File, Chunk_Size, Procs) ->
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    init_comp_proc(Procs, [], Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

init_comp_proc(Procs, List, Reader, Writer) ->
    case Procs of
        0 ->
            shell_loop(List, Reader, Writer);
        X ->
            Newpid = spawn(?MODULE, comp_workers_loop, [Reader, Writer, self()]),
            init_comp_proc(X-1, [Newpid|List],Reader,Writer)
    end.

shell_loop(PList, Reader, Writer) -> %%Loop executed by the shell process
    case PList of %%If all of the workers are done the list will be empty
        [] ->
            Reader! stop,
            Writer! stop,
            receive
                reader_done -> done
            end,
            receive
                writer_done -> done
            end;
        PList ->
            receive %%If a worker has finished, it shall be removed from the list
                {done,Pid} ->
                    shell_loop([X || X <- PList,X /= Pid],Reader,Writer);
                {error,Reason} -> %%Error, all processes should be killed
                    io:format("Error reading input file: ~w~n",[Reason]),
                    [X ! stop || X <- PList],
                    Reader ! stop,
                    Writer ! abort
            end
    end.

comp_workers_loop(Reader, Writer, Shell) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_workers_loop(Reader, Writer,Shell);
        eof ->  %% end of file, stop and send signal to shell process
            Shell ! {done,self()};
        {error, Reason} ->
            Shell ! {error, Reason};
        stop -> stop
    end.

%% File Decompression

decomp(Archive) ->
    decomp(Archive, string:replace(Archive, ".ch", "", trailing)).

decomp(Archive, Output_File) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    decomp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

decomp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer);
        eof ->    %% end of file => exit decompression
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.
