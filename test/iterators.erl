%% -------------------------------------------------------------------
%%
%%  eleveldb: Erlang Wrapper for LevelDB (http://code.google.com/p/leveldb/)
%%
%% Copyright (c) 2010-2013 Basho Technologies, Inc. All Rights Reserved.
%%
%% This file is provided to you under the Apache License,
%% Version 2.0 (the "License"); you may not use this file
%% except in compliance with the License.  You may obtain
%% a copy of the License at
%%
%%   http://www.apache.org/licenses/LICENSE-2.0
%%
%% Unless required by applicable law or agreed to in writing,
%% software distributed under the License is distributed on an
%% "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
%% KIND, either express or implied.  See the License for the
%% specific language governing permissions and limitations
%% under the License.
%%
%% -------------------------------------------------------------------
-module(iterators).

-compile(export_all).

-include_lib("eunit/include/eunit.hrl").

prev_test() ->
  os:cmd("rm -rf ltest"),  % NOTE
  {ok, Ref} = rocksdb:open("ltest", [{create_if_missing, true}]),
  try
    rocksdb:put(Ref, <<"a">>, <<"x">>, []),
    rocksdb:put(Ref, <<"b">>, <<"y">>, []),
    {ok, I} = rocksdb:iterator(Ref, []),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(I, <<>>)),
    ?assertEqual({ok, <<"b">>, <<"y">>},rocksdb:iterator_move(I, next)),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(I, prev)),
    ?assertEqual(ok, rocksdb:iterator_close(I))
  after
    rocksdb:close(Ref)
  end.

seek_for_prev_test() ->
  os:cmd("rm -rf ltest"),  % NOTE
  {ok, Ref} = rocksdb:open("ltest", [{create_if_missing, true}]),
  try
    rocksdb:put(Ref, <<"a">>, <<"1">>, []),
    rocksdb:put(Ref, <<"b">>, <<"2">>, []),
    rocksdb:put(Ref, <<"c">>, <<"3">>, []),
    rocksdb:put(Ref, <<"e">>, <<"4">>, []),

    {ok, I} = rocksdb:iterator(Ref, []),
    ?assertEqual({ok, <<"b">>, <<"2">>},rocksdb:iterator_move(I, {seek_for_prev, <<"b">>})),
    ?assertEqual({ok, <<"c">>, <<"3">>},rocksdb:iterator_move(I, {seek_for_prev, <<"d">>})),
    ?assertEqual({ok, <<"e">>, <<"4">>},rocksdb:iterator_move(I, next)),
    ?assertEqual(ok, rocksdb:iterator_close(I))
  after
    rocksdb:close(Ref)
  end.


cf_iterator_test() ->
  os:cmd("rm -rf ltest"),  % NOTE
  {ok, Ref, [DefaultH]} = rocksdb:open_with_cf("ltest", [{create_if_missing, true}], [{"default", []}]),
  {ok, TestH} = rocksdb:create_column_family(Ref, "test", []),
  try
    rocksdb:put(Ref, DefaultH, <<"a">>, <<"x">>, []),
    rocksdb:put(Ref, DefaultH, <<"b">>, <<"y">>, []),
    rocksdb:put(Ref, TestH, <<"a">>, <<"x1">>, []),
    rocksdb:put(Ref, TestH, <<"b">>, <<"y1">>, []),

    {ok, DefaultIt} = rocksdb:iterator(Ref, DefaultH, []),
    {ok, TestIt} = rocksdb:iterator(Ref, TestH, []),

    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, <<>>)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, <<>>)),
    ?assertEqual({ok, <<"b">>, <<"y">>},rocksdb:iterator_move(DefaultIt, next)),
    ?assertEqual({ok, <<"b">>, <<"y1">>},rocksdb:iterator_move(TestIt, next)),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, prev)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, prev)),
    ok = rocksdb:iterator_close(TestIt),
    ok = rocksdb:iterator_close(DefaultIt)
  after
    rocksdb:close(Ref)
  end.

cf_iterators_test() ->
  os:cmd("rm -rf ltest"),  % NOTE
  {ok, Ref, [DefaultH]} = rocksdb:open_with_cf("ltest", [{create_if_missing, true}], [{"default", []}]),
  {ok, TestH} = rocksdb:create_column_family(Ref, "test", []),
  try
    rocksdb:put(Ref, DefaultH, <<"a">>, <<"x">>, []),
    rocksdb:put(Ref, DefaultH, <<"b">>, <<"y">>, []),
    rocksdb:put(Ref, TestH, <<"a">>, <<"x1">>, []),
    rocksdb:put(Ref, TestH, <<"b">>, <<"y1">>, []),

    {ok, [DefaultIt, TestIt]} = rocksdb:iterators(Ref, [DefaultH, TestH], []),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, <<>>)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, <<>>)),
    ?assertEqual({ok, <<"b">>, <<"y">>},rocksdb:iterator_move(DefaultIt, next)),
    ?assertEqual({ok, <<"b">>, <<"y1">>},rocksdb:iterator_move(TestIt, next)),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, prev)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, prev)),
    ok = rocksdb:iterator_close(TestIt),
    ok = rocksdb:iterator_close(DefaultIt)
  after
    rocksdb:close(Ref)
  end.


drop_cf_with_iterator_test() ->
  os:cmd("rm -rf ltest"),  % NOTE
  {ok, Ref, [DefaultH]} = rocksdb:open_with_cf("ltest", [{create_if_missing, true}], [{"default", []}]),
  {ok, TestH} = rocksdb:create_column_family(Ref, "test", []),
  try
    rocksdb:put(Ref, DefaultH, <<"a">>, <<"x">>, []),
    rocksdb:put(Ref, DefaultH, <<"b">>, <<"y">>, []),
    rocksdb:put(Ref, TestH, <<"a">>, <<"x1">>, []),
    rocksdb:put(Ref, TestH, <<"b">>, <<"y1">>, []),

    {ok, DefaultIt} = rocksdb:iterator(Ref, DefaultH, []),
    {ok, TestIt} = rocksdb:iterator(Ref, TestH, []),

    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, <<>>)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, <<>>)),

    ok = rocksdb:drop_column_family(TestH),

    %% make sure we can read the read iterator
    ?assertEqual({ok, <<"b">>, <<"y">>},rocksdb:iterator_move(DefaultIt, next)),
    ?assertEqual({ok, <<"b">>, <<"y1">>},rocksdb:iterator_move(TestIt, next)),
    ?assertEqual({ok, <<"a">>, <<"x">>},rocksdb:iterator_move(DefaultIt, prev)),
    ?assertEqual({ok, <<"a">>, <<"x1">>},rocksdb:iterator_move(TestIt, prev)),
    ok = rocksdb:iterator_close(TestIt),
    ok = rocksdb:iterator_close(DefaultIt)

  after
    rocksdb:close(Ref)
  end.

