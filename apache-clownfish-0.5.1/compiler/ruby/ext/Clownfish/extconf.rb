# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

require 'mkmf'
require 'rbconfig'

CLOWNFISH_RUBY_DIR      = File.join('..','..')
CLOWNFISH_INCLUDE_DIR   = File.join('..','..','..','include')
CLOWNFISH_SRC_DIR       = File.join('..','..','..','src')
CLOWNFISH_RUNTIME       = File.join('..','..','..','..','runtime','ruby')
$CFLAGS = "-I#{CLOWNFISH_RUBY_DIR} -I#{CLOWNFISH_INCLUDE_DIR} -I#{CLOWNFISH_SRC_DIR} -I#{CLOWNFISH_RUNTIME}"
$objs = ['CFC.' + RbConfig::CONFIG['OBJEXT']]
obj_glob = File.join(CLOWNFISH_SRC_DIR, '*.' + RbConfig::CONFIG['OBJEXT'])
Dir.glob(obj_glob).each do|o|
    $objs.push o
end

create_makefile 'CFC'
