# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: pmu_sample.proto
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


import nanopb_pb2 as nanopb__pb2


DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x10pmu_sample.proto\x1a\x0cnanopb.proto\"d\n\npmu_sample\x12\n\n\x02ip\x18\x01 \x01(\x04\x12\x0b\n\x03pid\x18\x02 \x01(\r\x12\x0c\n\x04time\x18\x03 \x01(\x04\x12\x0b\n\x03\x63pu\x18\x04 \x01(\r\x12\x0e\n\x06period\x18\x05 \x01(\x04\x12\x12\n\x03ips\x18\x06 \x03(\x04\x42\x05\x92?\x02\x10\x04\x62\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'pmu_sample_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:
  DESCRIPTOR._options = None
  _globals['_PMU_SAMPLE'].fields_by_name['ips']._options = None
  _globals['_PMU_SAMPLE'].fields_by_name['ips']._serialized_options = b'\222?\002\020\004'
  _globals['_PMU_SAMPLE']._serialized_start=34
  _globals['_PMU_SAMPLE']._serialized_end=134
# @@protoc_insertion_point(module_scope)