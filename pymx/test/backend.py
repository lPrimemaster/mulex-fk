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
            '/system/metrics/cpu_usage',
            lambda key, val: print(key, val)
        )

        time.sleep(1)

        self.unwatch('/system/metrics/cpu_usage')

        print('Tesing event subscription...')

        self.subscribe('TestBackend::dummy', lambda data: print(data))

        self.log(TestBackend.LogType.INFO, 'Getting events from test_bck...')

        time.sleep(1)

        try:
            print(f'Result from RPC from test_bck: {
                self.call(
                    'test_bck',
                    ct.c_uint32(7),
                    ct.c_float(42),
                    fmt='f'
                )
            }')
        except Exception as e:
            print(e)

        time.sleep(1)

        self.unsubscribe('TestBackend::dummy')

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
