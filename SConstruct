import os

def env_base():
    env = DefaultEnvironment()
    env['CXXFLAGS'] = "-std=c++2a"
    env.Append(CXXFLAGS=["-g"])
    return env

parser_test_env = env_base()
parser_test_env.Append(CPPPATH = ['src/'])
parser_test_env.Append(CPPPATH = [os.environ['DOCTEST_HOME']])
parser_test_src = Glob("test/parser/*.cpp")
parser_test_env.Program("test_parser", parser_test_src)
