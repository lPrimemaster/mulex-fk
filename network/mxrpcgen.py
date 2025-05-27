#!/usr/bin/python3

# Author : César Godinho
# Date   : 11/10/2024
# Brief  : Detects and parses C++ RPC method declarations
#          (This is NOT a general purpose method parser)

from typing import List, Dict, Tuple
import re
import operator
import os
import argparse
import io
import sys
import json


class RPCMethodType:
    def __init__(self, typename: str, fulltypename: str):
        self.typename = typename
        self.fulltypename = fulltypename
        self._cached_inner_typename = None
        self._is_vector = None

    def is_type(self, typename: str) -> bool:
        return self.fulltypename == typename

    def is_reference(self) -> bool:
        return '&' in self.fulltypename

    def is_generic(self) -> bool:
        return self.is_type('mulex::RPCGenericType')


class RPCMethodArg:
    def __init__(self, name: str, typename: RPCMethodType):
        self.name = name
        self.typename = typename

class RPCMethodPermission:
    def __init__(self, name: str, description: str = ''):
        self.name = name
        self.description = description

class RPCMethodRole:
    def __init__(self, name: str, description: str, permissions: List[RPCMethodPermission]):
        self.name = name
        self.description = description
        self.permissions = permissions

# BUG: This does not support arguments that use
#      `using namespace x;` on their scope
#      For now just use the full typenames on
#      RPC method calls
class RPCMethodDetails:
    def __init__(self,
                 name: str,
                 fullname: str,
                 rettype: RPCMethodType,
                 args: List[RPCMethodArg],
                 perms: List[RPCMethodPermission]):
        self.name = name
        self.fullname = fullname
        self.rettype = rettype
        self.args = args
        self.perms = perms

    def __str__(self):
        print_lines = [
            f'Name: {self.name}',
            f'Fullname: {self.fullname}',
            'Return Type:',
            f'\tFull typename: {self.rettype.typename}',
            f'\t     Typename: {self.rettype.typename}',
            'Arguments:',
        ]
        for i, arg in enumerate(self.args):
            print_lines.append(f'\t[{i}]')
            print_lines.append(f'\t         Name: {arg.name}')
            print_lines.append(f'\tFull typename: {arg.typename.fulltypename}')
            print_lines.append(f'\t     Typename: {arg.typename.typename}\n')
        if not self.args:
            print_lines.append('\tNone')

        print_lines.append('Permissions:')
        for perm in self.perms:
            print_lines.append(f'\t{perm.name}')
        if not self.perms:
            print_lines.append('\tNone')

        return '\n'.join(print_lines)

class RPCRoleGenerator:
    def __init__(self, config_file: str):
        self._parse_file(config_file)
        self.buffer = io.StringIO()

    def _parse_file(self, file: str) -> None:
        with open(file, 'r') as f:
            data = json.load(f)

            self.roles = []
            for role in data['roles']:
                self.roles.append(RPCMethodRole(
                    role['name'],
                    role['description'],
                    [RPCMethodPermission(p) for p in role['permissions']]
                ))

            self.permissions = []
            for role in data['permissions']:
                self.permissions.append(RPCMethodPermission(
                    role['name'],
                    role['description']
                ))

    def _write_indented(self, indent: int, value: str) -> None:
        self.buffer.write('\t'*indent)
        self.buffer.write(value)

    def _write_file(self, filename: str) -> None:
        with open(filename, 'w') as f:
            f.write(self.buffer.getvalue())

    def _generate_table_insert(self,
                               name: str,
                               descriptors: List[str],
                               values: List[List[str]]):

        if not values:
            raise Exception('[mxrpcgen] Invalid table generation.')

        self._write_indented(
            0,
            f"INSERT INTO {name} ({', '.join(descriptors)}) VALUES\n"
        )

        for v in values[:-1]:
            self._write_indented(1, f"({', '.join(v)}),\n")

        self._write_indented(1, f"({', '.join(values[-1])});\n")

    def _stringify(self, value: str) -> str:
        return f'"{value}"'

    def _get_cross_idx(self, value: str):
        return next(
            (
                i for i, d in enumerate(self.permissions) 
                if d.name == value
            ), 
            -1
        )

    def _flatten_rjl(self, rjl: List) -> List[List[str]]:
        return sum([i for i in [j for j in [k for k in rjl]]], [])

    def _get_permission_role_join_list(self) -> List[List[str]]:
        rjl = [
            [
                [
                    f'{p[0] + 1}',
                    f'{self._get_cross_idx(pv.name) + 1}'
                ]
                for pv in p[1]
            ]
            for p in [(i, r.permissions) for i, r in enumerate(self.roles)]
        ]
        return self._flatten_rjl(rjl)

    def generate_sql(self, filename: str) -> None:
        print('[mxpdbgen] Generating SQL init script.')

        self._write_indented(0, "PRAGMA foreign_keys = ON;\n")

        print('[mxpdbgen] Generating table: roles.')
        self._write_indented(
            0,
            "CREATE TABLE IF NOT EXISTS roles (\n"
                "\tid INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "\tname TEXT UNIQUE NOT NULL,\n"
                "\tdescription TEXT\n"
            ");\n"
        )

        print('[mxpdbgen] Generating table: permissions.')
        self._write_indented(
            0,
            "CREATE TABLE IF NOT EXISTS permissions (\n"
                "\tid INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "\tname TEXT UNIQUE NOT NULL,\n"
                "\tdescription TEXT\n"
            ");\n"
        )

        print('[mxpdbgen] Generating table: rolepermissions.')
        self._write_indented(
            0,
            "CREATE TABLE IF NOT EXISTS rolepermissions (\n"
                "\trole_id INTEGER,\n"
                "\tpermission_id INTEGER,\n"
                "\tPRIMARY KEY (role_id, permission_id),\n"
                "\tFOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,\n"
                "\tFOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE CASCADE\n"
            ");\n"
        )

        print('[mxpdbgen] Generating table: users.')
        self._write_indented(
            0,
            "CREATE TABLE IF NOT EXISTS users (\n"
                "\tid INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "\tusername TEXT UNIQUE NOT NULL,\n"
                "\tsalt TEXT NOT NULL,\n"
                "\tpasshash TEXT NOT NULL,\n"
                "\trole_id INTEGER,\n"
                "\tcreated_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
                "\tFOREIGN KEY (role_id) REFERENCES roles(id)\n"
            ");\n"
        )

        print('[mxpdbgen] Generating table inserts.')
        try:
            self._generate_table_insert(
                'permissions',
                ['name', 'description'],
                [
                    [
                        self._stringify(p.name),
                        self._stringify(p.description)
                    ]
                    for p in self.permissions
                ]
            )

            self._generate_table_insert(
                'roles',
                ['name', 'description'],
                [
                    [
                        self._stringify(r.name),
                        self._stringify(r.description)
                    ]
                    for r in self.roles
                ]
            )

            self._generate_table_insert(
                'rolepermissions',
                ['role_id', 'permission_id'],
                self._get_permission_role_join_list()
            )
        except Exception as e:
            print(e)
            sys.exit(1)

        self._write_file(filename)

    def get_permissions(self) -> List[RPCMethodPermission]:
        return self.permissions

    def get_roles(self) -> List[RPCMethodRole]:
        return self.roles

class RPCFileParser:
    def __init__(self, filenames: List[str], role_gen: RPCRoleGenerator | None = None):
        # Define all the variables to parse the file
        self.rpc_call_keyword = '${RPC_CALL_KEYWORD}'
        self.rpc_perm_keyword = '${RPC_PERM_KEYWORD}'
        self.filenames = filenames
        self.rpc_methods = {}

        if role_gen:
            self.rpc_permissions = role_gen.get_permissions()
        else:
            self.rpc_permissions = []

    def parse(self) -> Dict[str, List[RPCMethodDetails]]:
        total_calls = 0
        for file in self.filenames:
            self.rpc_methods[file] = self._parse_file(file)
            total_calls += len(self.rpc_methods[file])
        print(f'[mxrpcgen] Done. Total parsed RPC calls: {total_calls}')
        return self.rpc_methods

    def _load_file(self, filename: str) -> List[str]:
        with open(filename, 'r', encoding='utf-8') as f:
            return f.readlines()

    def _parse_file(self, filename: str) -> List[RPCMethodDetails]:
        lines = self._load_file(filename)
        def_lines = self._find_def_lines(lines)
        scopes = self._find_lines_scope(lines, def_lines)
        rpc_methods = []
        for line, scope in scopes.items():
            method_details = self._find_rpc_declaration(lines[line], scope)
            rpc_methods.append(method_details)
            print(f'[mxrpcgen] {filename}: '
                  f'registering method: {method_details.fullname}')
            # print(method_details)
        return rpc_methods

    def _find_def_lines(self, lines: List[str]) -> List[int]:
        lines_with_def = []
        for i, line in enumerate(lines):
            if self.rpc_call_keyword in line and not line.strip().startswith('//'):
                lines_with_def.append(i)
        return lines_with_def

    # BUG: This does not work for scenarios like
    #      `namespace x { } CALL_DESC <func_dec>;`
    #      or
    #      `namespace x { class y { CALL_DESC <func_dec>; } }`
    def _find_lines_scope(self, lines: List[str], indices: List[int]) -> Dict[int, str]:
        scope_dict = {}
        scope_name_stack = []
        scope_stack = 0

        for i, line in enumerate(lines):
            for kw in ['namespace', 'class', 'struct']:
                match = re.findall(fr'^[ \t]*{kw} +([a-zA-Z0-9_]*)', line)
                if match:
                    # This is a forward declaration: skip it
                    if re.findall(fr'^[ \t]*{kw} +([a-zA-Z0-9_]*);', line):
                        continue
                    scope_name_stack.append((match[0], scope_stack + 1))

                if '{' in line:
                    scope_stack += 1
                if '}' in line:
                    if len(scope_name_stack) > 0:
                        if scope_name_stack[-1][1] == scope_stack:
                            scope_name_stack.pop()
                    scope_stack -= 1

                if match:
                    break
            if i in indices:
                scope_dict[i] = '::'.join(
                    map(operator.itemgetter(0), scope_name_stack)
                )

        return scope_dict

    def _find_rpc_declaration(self, line: str, scope: str) -> RPCMethodDetails:
        declaration = re.findall(
            fr'^[ \t]*{self.rpc_call_keyword} +(?:{self.rpc_perm_keyword}\((.*)\) +)?(.+) +(.+)\((.*)\)',
            line
        )

        try:
            permissions = self._parse_permissions(declaration[0][0])
        except Exception as e:
            print(e)
            print(f'[mxrpcgen]\tat: {line.strip()}')
            sys.exit(1)

        try:
            return_type = self._parse_return_type(declaration[0][1])
        except Exception as e:
            print(e)
            print(f'[mxrpcgen]\tat: {line.strip()}')
            sys.exit(1)

        method_name = declaration[0][2].strip()

        try:
            arguments = self._parse_arguments(declaration[0][3])
        except Exception as e:
            print(e)
            print(f'[mxrpcgen]\tat: {line.strip()}')
            sys.exit(1)

        if scope:
            return RPCMethodDetails(
                method_name,
                f'{scope}::{method_name}',
                return_type,
                arguments,
                permissions
            )

        return RPCMethodDetails(
            method_name,
            method_name,
            return_type,
            arguments,
            permissions
        )

    def _parse_return_type(self, return_type: str) -> RPCMethodType:
        typenames = return_type.split()
        rtype = RPCMethodType(
            typenames[-1].strip(),
            ' '.join(typenames).strip()
        )

        if rtype.is_reference():
            raise Exception('[mxrpcgen] Error, return types '
                            'can not be references.')
        return rtype

    def _parse_permissions(self, permissions: str) -> List[RPCMethodPermission]:
        perm_parsed = []
        permission_names = [p.name for p in self.rpc_permissions]
        if permissions:
            for perm in permissions.split(','):
                pfmt = perm.strip().replace('"', '')
                perm_parsed.append(RPCMethodPermission(pfmt))
                if pfmt not in permission_names:
                    raise Exception('[mxrpcgen] Error, rpc permission '
                                    f'"{pfmt}" is invalid.')
        return perm_parsed

    def _parse_arguments(self, arguments: str) -> List[RPCMethodArg]:
        args_parsed = []
        if arguments:
            for arg in arguments.split(','):
                kwords = arg.strip().split()
                typenames = kwords[:-1]
                name = kwords[-1]
                args_parsed.append(RPCMethodArg(
                    name,
                    RPCMethodType(typenames[-1], ' '.join(typenames))
                ))
                if args_parsed[-1].typename.is_reference():
                    raise Exception('[mxrpcgen] Error, parameter types '
                          'can not be references.')
        return args_parsed


class RPCGenerator:
    def __init__(self, methods: Dict[str, List[RPCMethodDetails]]):
        self.buffer = io.StringIO()
        self.methods = methods

    def _write_newline(self) -> None:
        self.buffer.write('\n')

    def _write_indented(self, indent: int, value: str) -> None:
        self.buffer.write('\t'*indent)
        self.buffer.write(value)

    def _reset_buffer(self) -> None:
        self.buffer = io.StringIO()

    def _generate_method_id_name(self, fullname: str) -> str:
        return 'RPC_CALL_' + fullname.replace('::', '_').upper()

    def _generate_includes(self) -> None:
        anylen = False
        self.buffer.write('#include <cstdint>\n')
        self.buffer.write('#include <vector>\n')
        self.buffer.write('#ifdef TRACY_ENABLE\n')
        self.buffer.write('#include <tracy/Tracy.hpp>\n')
        self.buffer.write('#else\n')
        self.buffer.write('#define ZoneScoped\n')
        self.buffer.write('#endif\n')
        self._write_newline()
        for file, methods in self.methods.items():
            if len(methods):
                anylen = True
                abs_path = os.path.abspath(file)
                self.buffer.write(f'#include "{abs_path}"\n')
        if anylen:
            self._write_newline()

    def _generate_ids(self) -> List[Tuple[RPCMethodDetails, int, int]]:
        id_table = []
        mids = []
        id = 0
        anym = False

        for methods in self.methods.values():
            for method in methods:
                anym = True
                mid = self._generate_method_id_name(method.fullname)
                mids.append((method, mid))

        for method, mid in sorted(mids, key=lambda k: k[1]):
            id_table.append((method, mid, id))
            self.buffer.write(f'#define {mid} {id}\n')
            id += 1

        if anym:
            self._write_newline()
        return id_table

    def _generate_name_lookup(self, idt: List[Tuple[RPCMethodDetails, int, int]]) -> None:
            # Generate a std::map to look for the key (might be slower for less entries)
            self._write_newline()
            self._write_indented(0, 'namespace\n')
            self._write_indented(0, '{\n')
            self._write_indented(1, 'std::uint16_t RPCGetMethodId(const std::string& name)\n')
            self._write_indented(1, '{\n')
            self._write_indented(2, 'static const std::unordered_map<std::string, std::uint16_t> _map = {\n')
            for mname, _, id in idt:
                self._write_indented(3, f'{{\"{mname.fullname}\", {id}}},\n')
            self._write_indented(2, '};\n')
            self._write_indented(2, 'auto it = _map.find(name);\n')
            self._write_indented(2, 'if(it != _map.end())\n')
            self._write_indented(2, '{\n')
            self._write_indented(3, 'return it->second;\n')
            self._write_indented(2, '}\n')
            self._write_indented(2, 'return static_cast<std::uint16_t>(-1);\n')
            self._write_indented(1, '}\n')
            self._write_indented(0, '}\n')
            self._write_newline()

    def _generate_name_list(self, idt: List[Tuple[RPCMethodDetails, int, int]]) -> None:
        # Generate a std::vector with all the keys
        self._write_newline()
        self._write_indented(0, 'namespace\n')
        self._write_indented(0, '{\n')
        self._write_indented(1, 'std::vector<std::string> RPCGetMethods()\n')
        self._write_indented(1, '{\n')
        self._write_indented(2, 'static const std::vector<std::string> _list = {\n')
        for mname, _, _ in idt:
            self._write_indented(3, f'\"{mname.fullname}\",\n')
            self._write_indented(3, f'\"{mname.rettype.fulltypename}\",\n')
        self._write_indented(2, '};\n')
        self._write_indented(2, 'return _list;\n')
        self._write_indented(1, '}\n')
        self._write_indented(0, '}\n')
        self._write_newline()

    def _generate_case(self, idt: Tuple[RPCMethodDetails, int, int]) -> None:
        method, mid, _ = idt
        self._write_indented(3, f'case {mid}:\n')
        self._write_indented(3, '{\n')

        # Static assertion checks for trivially copyable
        # for non void and non generic types
        ret_is_void = method.rettype.is_type('void')
        ret_is_gene = method.rettype.is_generic()
        if not ret_is_void and not ret_is_gene:
            self._write_indented(
                4, 'static_assert(std::is_trivially_copyable_v<'
                   f'{method.rettype.fulltypename}>, "RPC return type must be '
                   'trivially copyable. But is of type: '
                   f'{method.rettype.fulltypename}");\n')

        if len(method.args) > 0:
            for arg in method.args:
                if not arg.typename.is_generic():
                    self._write_indented(
                        4, 'static_assert(std::is_trivially_copyable_v<'
                           f'{arg.typename.fulltypename}>, '
                           '"RPC parameter type must be '
                           'trivially copyable. But is of type: '
                           f'{arg.typename.fulltypename}");\n')
            # We require args, nullptr guard
            self._write_indented(4, 'if(!args) return {};\n')

        # Argument offsets
        if len(method.args) > 1:
            for i, arg in enumerate(method.args[:-1]):
                if i == 0:
                    if arg.typename.is_generic():
                        self._write_indented(
                            4,
                            'const std::uint64_t o1 = '
                            f'*reinterpret_cast<const std::uint64_t*>(args) + sizeof(std::uint64_t);\n'
                        )
                    else:
                        self._write_indented(
                            4,
                            'constexpr std::uint64_t o1 = '
                            f'sizeof({arg.typename.fulltypename});\n'
                        )
                else:
                    if arg.typename.is_generic():
                        self._write_indented(
                            4,
                            f'const std::uint64_t o{i + 1} = o{i} + '
                            f'*reinterpret_cast<const std::uint64_t*>(args + o{i}) + sizeof(std::uint64_t);\n'
                        )
                    else:
                        self._write_indented(
                            4,
                            f'constexpr std::uint64_t o{i + 1} = o{i} + '
                            f'sizeof({arg.typename.fulltypename});\n'
                        )

        # Return type (except void and generic)
        if not ret_is_void and not ret_is_gene:
            self._write_indented(
                4, 'retbuf.resize(sizeof('
                   f'{method.rettype.fulltypename}));\n'
            )
            self._write_indented(
                4, f'{method.rettype.fulltypename}* r = '
                   f'reinterpret_cast<{method.rettype.fulltypename}*>'
                   '(retbuf.data());\n'
            )

        # Invokes the copy constructor
        if ret_is_void:
            self._write_indented(4, f'{method.fullname}(')
        elif ret_is_gene:
            self._write_indented(4, f'mulex::RPCGenericType r = {method.fullname}(')
        else:
            self._write_indented(4, f'*r = {method.fullname}(')
        if len(method.args) > 0:
            self._write_indented(0, '\n')
            for i, arg in enumerate(method.args):
                # Appropriately set the const keyword
                constkw = ''
                if 'const' not in arg.typename.fulltypename:
                    constkw = 'const '

                if i == 0:
                    if arg.typename.is_generic():
                        self._write_indented(
                            5,
                            'mulex::RPCGenericType::FromData(args + sizeof(std::uint64_t), '
                            '*reinterpret_cast<const std::uint64_t*>(args))'
                        )
                    else:
                        self._write_indented(
                            5,
                            '*reinterpret_cast<'
                            f'{constkw}{arg.typename.fulltypename}'
                            '*>(args)'
                        )
                else:
                    if arg.typename.is_generic():
                        self._write_indented(
                            5,
                            f'mulex::RPCGenericType::FromData(args + o{i} + sizeof(std::uint64_t), '
                            f'*reinterpret_cast<const std::uint64_t*>(args + o{i}))'
                        )
                    else:
                        self._write_indented(
                            5,
                            '*reinterpret_cast<'
                            f'{constkw}{arg.typename.fulltypename}'
                            f'*>(args + o{i})'
                        )
                if i != len(method.args) - 1:
                    self._write_indented(0, ',\n')
                else:
                    self._write_indented(0, '\n')
            self._write_indented(4, ');\n')
        else:
            self._write_indented(0, ');\n')

        if ret_is_gene:
            self._write_indented(4, 'std::uint64_t data_size = r._data.size();\n')
            self._write_indented(4, 'retbuf.resize(data_size + sizeof(std::uint64_t));\n')
            self._write_indented(4, 'std::memcpy(retbuf.data(), &data_size, sizeof(std::uint64_t));\n')
            self._write_indented(4, 'std::memcpy(retbuf.data() + sizeof(std::uint64_t), r._data.data(), data_size);\n')

        self._write_indented(4, 'break;\n')
        self._write_indented(3, '};\n')

    def _generate_call_lookup(self,
                              ids: List[Tuple[RPCMethodDetails, int, int]]):
        self.buffer.write('namespace\n')
        self.buffer.write('{\n')
        self._write_indented(
            1,
            'std::vector<std::uint8_t> RPCCallLocally(std::uint16_t pid, '
            'const std::uint8_t* args)\n')
        self._write_indented(1, '{\n')
        self._write_indented(2, 'ZoneScoped;\n')
        self._write_indented(2, 'std::vector<std::uint8_t> retbuf;\n')
        self._write_indented(2, 'switch(pid)\n')
        self._write_indented(2, '{\n')

        for id in ids:
            self._generate_case(id)

        self._write_indented(2, '}\n')
        self._write_indented(2, 'return retbuf;\n')
        self._write_indented(1, '}\n')
        self.buffer.write('}\n')

    def _generate_message(self) -> None:
        self._write_indented(0, '// ####################\n')
        self._write_indented(0, '// #  File generated  #\n')
        self._write_indented(0, '// #  by mxrpcgen.py  #\n')
        self._write_indented(0, '// #  DO NOT MODIFY!  #\n')
        self._write_indented(0, '// ####################\n')
        self._write_newline()

    def _generate_perm_definitions(self, permissions: List[RPCMethodPermission]) -> Dict[str, int]:
        output = {}
        for i, perm in enumerate(permissions):
            output[perm.name] = i
            self._write_indented(0, f'#define {perm.name.upper()} {i}\n')
        self._write_newline()
        return output

    def _calculate_permission(self, permap: Dict[str, int], permissions: List[RPCMethodPermission]):
        # Calculate PdbPermissions
        lo = 0
        hi = 0
        for p in permissions:
            i = permap[p.name]
            if i < 64:
                # Set to lo
                lo |= 1 << i
            else:
                # Set to hi
                hi |= 1 << (i - 64)
        return hi, lo

    def _generate_perm_lookup(self,
                              roles: List[RPCMethodRole],
                              permap: Dict[str, int],
                              idt: List[Tuple[RPCMethodDetails, int, int]]) -> None:
        self._write_indented(0, 'inline static const std::unordered_map<std::string, mulex::PdbPermissions> _role_lookup = {\n')

        lines = []
        for role in roles:
            hi, lo = self._calculate_permission(permap, role.permissions)
            constructor = f'mulex::PdbPermissions({hi}, {lo})'
            lines.append(f'\t{{ "{role.name}",  {constructor} }}')

        self._write_indented(0, ',\n'.join(lines))
        self._write_indented(0, '\n};\n')
        self._write_newline()

        self._write_indented(0, 'inline mulex::PdbPermissions PdbGetUserPermissions(const std::string& username)\n')
        self._write_indented(0, '{\n')
        self._write_indented(1, 'auto user = _role_lookup.find(username);\n')
        self._write_indented(1, 'if(user != _role_lookup.end())\n')
        self._write_indented(1, '{\n')
        self._write_indented(2, 'return user->second;\n')
        self._write_indented(1, '}\n')
        self._write_indented(1, 'return mulex::PdbPermissions(0, 0);\n')
        self._write_indented(0, '}\n')
        self._write_newline()
        
        self._write_indented(0, 'inline bool PdbCheckMethodPermissions(std::uint16_t pid, const mulex::PdbPermissions& perm)\n')
        self._write_indented(0, '{\n')
        self._write_indented(1, 'if(perm.test(1)) return true; // SUPER always has permissions\n\n')
        self._write_indented(1, 'switch(pid)')
        self._write_indented(1, '{\n')

        for method, mid, _ in idt:
            hi, lo = self._calculate_permission(permap, method.perms)
            if hi == 0 and lo == 0:
                # No permissions return true
                self._write_indented(2, f'case {mid}: return true;\n')
            else:
                self._write_indented(2, f'case {mid}: return perm.test({hi}, {lo});\n')
        self._write_indented(1, '}\n')
        self._write_indented(0, '}\n')

    def _generate_perm_includes(self) -> None:
        self.buffer.write('#include <cstdint>\n')
        self.buffer.write('#include <vector>\n')
        self.buffer.write('#include "${CMAKE_SOURCE_DIR}/mxrdb.h"\n')
        self.buffer.write('#ifdef TRACY_ENABLE\n')
        self.buffer.write('#include <tracy/Tracy.hpp>\n')
        self.buffer.write('#else\n')
        self.buffer.write('#define ZoneScoped\n')
        self.buffer.write('#endif\n')
        self._write_newline()

    def _write_file(self, filename: str) -> None:
        with open(filename, 'w') as f:
            f.write(self.buffer.getvalue())

    def generate_rpc_header(self, filename: str) -> None:
        self._reset_buffer()
        self._generate_message()
        self._generate_includes()
        self.ids = self._generate_ids()
        self._generate_name_lookup(self.ids)
        self._generate_name_list(self.ids)
        self._generate_call_lookup(self.ids)
        self._write_file(filename)

    def generate_perm_header(self, filename: str, role_generator: RPCRoleGenerator) -> None:
        self._reset_buffer()
        self._generate_message()
        self._generate_perm_includes()
        permap = self._generate_perm_definitions(role_generator.get_permissions())
        self._generate_perm_lookup(role_generator.get_roles(), permap, self.ids)
        self._write_file(filename)

def get_files(args: argparse.Namespace) -> List[str]:
    files_to_parse = []
    for dir in args.dirs:
        for root, _, filenames in os.walk(args.dirs[0]):
            ignoreDir = False
            for ignore in args.ignore:
                if ignore in os.path.basename(root) or ignore in root:
                    ignoreDir = True
                    break
            if ignoreDir:
                continue

            for file in filenames:
                if file.lower().endswith(tuple(args.extensions)):
                    files_to_parse.append(os.path.join(root, file))

            # Only look at root directly
            if not args.recursive:
                break
    return files_to_parse


if __name__ == '__main__':
    arg_parser = argparse.ArgumentParser(
        prog='mxrpcgen',
        description='Generates RPC calls declarators and code logic.'
    )

    arg_parser.add_argument('--extensions', nargs='+', default=['.h'],
                            help='Specifies which file extensions '
                            'to look for.')
    arg_parser.add_argument('--dirs', nargs='+', default=['.'],
                            help='Specifies which directories to '
                            'search for `extensions`.')
    arg_parser.add_argument('--recursive',
                            action='store_true',
                            help='Recursively finds files under '
                            'nested directories of `dirs`.')
    arg_parser.add_argument('--ignore', nargs='+', default=[],
                            help='Specifies folder names to '
                            'ignore. Similar to .gitignore.')

    arg_parser.add_argument('--output-file', required=True,
                            help='Specifies the output filename.')

    arg_parser.add_argument('--sql-output', required=False,
                           help='Specifies the sql output file to '
                           'populate the user database.')
    arg_parser.add_argument('--permissions-input', required=False,
                           help='Specifies the json input file to '
                           'generate the SQL database from.'
                           )
    arg_parser.add_argument('--permissions-check-output', required=False,
                           help='Specifies the output file to '
                           'generate the C++ permission check '
                           'header to.'
                           )

    args = arg_parser.parse_args()
    files_to_parse = get_files(args)
    print('[mxrpcgen] Files to parse:')
    for pfile in files_to_parse:
        print(f'[mxrpcgen] \t{pfile}')

    if args.permissions_input and args.sql_output:
        role_generator = RPCRoleGenerator(args.permissions_input)
        role_generator.generate_sql(args.sql_output)
    else:
        role_generator = None

    file_parser = RPCFileParser(files_to_parse, role_generator)
    rpc_methods = file_parser.parse()
    generator = RPCGenerator(rpc_methods)
    generator.generate_rpc_header(args.output_file)

    if args.permissions_check_output and role_generator:
        generator.generate_perm_header(args.permissions_check_output, role_generator)
