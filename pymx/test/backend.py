#!/bin/python3.12
# from pymx import backend
from pymx.backend import Backend
import logging
import time
import ctypes as ct


class TestBackend(Backend):
    def __init__(self):
        logging.basicConfig(level=logging.DEBUG)
        super().__init__(__file__)

    def work(self):
        self.log(TestBackend.LogType.INFO, 'Info from my backend')

        self.create('/user/mykey', 0)
        self.write('/user/mykey', ct.c_uint8(127))
        print(self.read('/user/mykey'))

        # self.subscribe('test_evt', lambda data: print(data))

        self.watch(
            '/user/mykey',
            lambda key, val: print(key, val)
        )

        time.sleep(1)

        # self.delete('/user/mykey')

        self.unwatch('/user/mykey')

        print('Tesing event subscription...')

        # self.subscribe('TestBackend::dummy', lambda data: print(data))

        self.log(TestBackend.LogType.INFO, 'Getting events from test_bck...')

        time.sleep(1)

        try:
            result = self.call(
                    'test_bck.exe',
                    ct.c_uint32(7),
                    ct.c_float(42),
                    fmt='f'
                )
            print(f'Result from RPC from test_bck: {result}')
        except Exception as e:
            print(e)

        time.sleep(1)

        # self.unsubscribe('TestBackend::dummy')

    def rpc(self, p0: ct.c_int32, p1: ct.c_float):
        print(f'Called RPC function with data: {p0} {p1}')
        return ct.c_float(3.1415)


with TestBackend() as bck:
    bck.work()
    while True:
        try:
            time.sleep(0.2)
        except KeyboardInterrupt:
            break
