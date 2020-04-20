import threading


class UfmThread(object):
    def __init__(self):
        self.shutdown = False

        self.cv = threading.Condition()
        self.thread = threading.Thread(target=self.__run, args=())

    def __run(self):
        with self.cv:
            while not self.cv.wait_for(lambda: self.shutdown == True, self.repeatIntervalSecs):
                if self.cb != None:
                    try:
                        self.cb(self.cbArgs)
                    except:
                        pass

    def start(self, threadName='UfmThread', cb=None, cbArgs=None, repeatIntervalSecs=2.0):
        self.threadName = threadName
        self.cb = cb
        self.cbArgs = cbArgs
        self.repeatIntervalSecs = repeatIntervalSecs
        self.thread.start()

    def stop(self):
        self.shutdown = True
        with self.cv:
            self.cv.notify()
        self.thread.join()
