#!/usr/bin/env python3

from os import environ

class Response:
    Header:dict[str, str]
    Body:str
    def __init__(self):
        self.header = {}
        self.body = ""
        self.addHeader("Content-Type", "text/html")
        pass

    def importEnvheaders(self, queryString:str):
        queryString.split('&')

    def setResponseCode(self, code:int):
        self.addHeader("Status", str(code))
    
    def addHeader(self, key:str, value:str):
        self.header[key] = value
        pass

    def appBody(self, message:str):
        self.body = self.body + message
    
    def output(self):
        for key, value in self.Header.items():
            print (f"{key}:{value}")
        print()
        print(self.body)

if __name__ == "__main__":