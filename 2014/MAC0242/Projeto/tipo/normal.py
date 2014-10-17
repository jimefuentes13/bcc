#! /usr/bin/env python3

from tipo import Tipo

class Normal(Tipo):
    """ Normal."""
    def __init__(self):
        self.nome = "Normal"
        self.efetivo = set()
        self.nao_efetivo = set(['Rock'])
        self.sem_efeito = set(["Ghost"])
        
if __name__ == '__main__':
    pass