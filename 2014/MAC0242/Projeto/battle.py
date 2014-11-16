# Para manipular a linha de comando
import os, sys, getopt, re

# Para manipular e validar arquivos XML
from lxml import etree
from lxml.etree import XMLSyntaxError, Element

# Quem é este pokemon?
from pokemon.duel import Duel
from pokemon.pokemon import Pokemon

# Battle state schema file
bss = 'battle_state.xsd'

def validate(s):
    bsv = open(bss, "r").read()
    xml = re.sub("encoding=['\"].*['\"]", "", s)
    schema_root = etree.XML(bsv)
    schema = etree.XMLSchema(schema_root)
    parser = etree.XMLParser(schema = schema)
    try:
        root = etree.fromstring(xml, parser)
    except XMLSyntaxError:
        print("Formato XML incorreto!")
        return None

    return root.findall("pokemon")

def make_battle_state(pk1, pk2=None):
    top = Element('battle_state')
    x1 = pk1.make_xml()
    top.append(x1)
    if pk2 is not None:
        x2 = pk2.make_xml()
        top.append(x2)
    return etree.tostring(top, xml_declaration=True, pretty_print=True, encoding="UTF-8").decode("utf-8")

def simple_duel(patt, pdef, n=None, run=True):
    an = None
    if patt.get_HP() > 0 and pdef.get_HP() > 0:
        params = {
            "name1":patt.get_name(),
            "name2":pdef.get_name(),
            "hp1":patt.get_HP(),
            "hp2":pdef.get_HP(),
        }
        print("\n%(hp1)d\t- %(name1)s" % params)
        print("%(hp2)s\t- %(name2)s\n" % params)
        if n is not None:
            an = int(n)
            a = patt.select_attack(an)
        else:
            a = None
        if patt.left_pp() > 0:
            print("\nAtaques de", patt.get_name())
            patt.print_attack()
            while a is None:
                try:
                    an = int(input("Selecione um ataque para " + patt.get_name() + ": "))
                    a = patt.select_attack(an)
                except ValueError:
                    print("Digite um número entre 1 e", patt.get_nattack())
                if a is None:
                    print("Digite um número entre 1 e", patt.get_nattack())
        else:
            print("%(name)s has no moves left!" % {"name": patt.get_name()})
            an = 0
            a = patt.select_attack(0)
        if run:
            a.prepare(pdef)
            a.action()
            if pdef.get_HP() == 0:
                print("%s fainted!" % pdef.get_name())
            if patt.get_HP() == 0:
                print("%s fainted!" % patt.get_name())
            if pdef.get_HP()==0 or patt.get_HP()==0:
                print("\nBatalha encerrada!")
    return an

def load_billpc(dir):
    print("Temos os seguintes pokemons disponíveis:")
    bsv = open(bss, "r").read()
    schema_root = etree.XML(bsv)
    schema = etree.XMLSchema(schema_root)
    parser = etree.XMLParser(schema = schema)

    pokemons = []
    for file in os.listdir(dir):
        try:
            root = etree.fromstring(open(dir+file, "r").read(), parser)
        except XMLSyntaxError:
            print("Formato XML incorreto!")
            continue
        pokemons.append(Pokemon())
        pokemons[-1].load_xml(root.find("pokemon"))

    for i in range(len(pokemons)):
        params = {"n":i+1,"name":pokemons[i].get_name(), "level": pokemons[i].get_level()}
        print("%(n)d - [%(level)d] %(name)s" % params)

    print()
    p1 = -1
    p2 = -1
    while not (p1 >= 0 and p1 < len(pokemons)):
        try:
            p1 = int(input("Digite o número do primeiro pokemon selecionado: ")) -1
        except ValueError:
            print("Digite um número entre 1 e", len(pokemons))
        except (EOFError, KeyboardInterrupt):
            print()
            return (None, None)
    while not (p2 >= 0 and p2 < len(pokemons)):
        try:
            p2 = int(input("Digite o número do segundo pokemon selecionado: ")) -1
        except ValueError:
            print("Digite um número entre 1 e", len(pokemons))
    return (pokemons[p1], pokemons[p2])


def load_billpc2(dir):
    print("Temos os seguintes pokemons disponíveis:")
    bsv = open(bss, "r").read()
    schema_root = etree.XML(bsv)
    schema = etree.XMLSchema(schema_root)
    parser = etree.XMLParser(schema = schema)

    pokemons = []
    for file in os.listdir(dir):
        try:
            root = etree.fromstring(open(dir+file, "r").read(), parser)
        except XMLSyntaxError:
            print("Formato XML incorreto!")
            continue
        pokemons.append(Pokemon())
        pokemons[-1].load_xml(root.find("pokemon"))

    for i in range(len(pokemons)):
        params = {"n":i+1,"name":pokemons[i].get_name(), "level": pokemons[i].get_level()}
        print("%(n)d - [%(level)d] %(name)s" % params)

    p1 = -1
    while not (p1 >= 0 and p1 < len(pokemons)):
        try:
            p1 = int(input("Digite o número do pokemon selecionado: ")) -1
        except ValueError:
            print("Digite um número entre 1 e", len(pokemons))
        except (EOFError, KeyboardInterrupt):
            print()
            return (None, None)
    return pokemons[p1]

def load_keyboard():
    p1 = Pokemon()
    p2 = Pokemon()
    p1.load()
    p2.load()
    return (p1, p2)

def battle(p1, p2):
    d = Duel()

    while p1.get_HP() > 0 and p2.get_HP() > 0:
        params = {
            "name1":p1.get_name(),
            "name2":p2.get_name(),
            "hp1":p1.get_HP(),
            "hp2":p2.get_HP(),
        }
        print("\n%(hp1)d\t- %(name1)s" % params)
        print("%(hp2)s\t- %(name2)s\n" % params)
        
        if p1.left_pp() > 0:
            print("\nAtaques de", p1.get_name())
            p1.print_attack()
            a1 = None
            while a1 is None:
                try:
                    a1 = p1.select_attack(int(input("Selecione um ataque para " + p1.get_name() + ": ")))
                except ValueError:
                    print("Digite um número entre 1 e", p1.get_nattack())
                except EOFError:
                    a1 = p1.select_attack(int(input()))
                if a1 is None:
                    print("Digite um número entre 1 e", p1.get_nattack())
        else:
            print("%(name)s has no moves left!" % {"name": p1.get_name()})
            a1 = p1.select_attack(0)
        
        if p2.left_pp() > 0:
            print("\nAtaques de", p2.get_name())
            p2.print_attack()
            a2 = None
            while a2 is None:
                try:
                    a2 = p2.select_attack(int(input("Selecione um ataque para " + p2.get_name() + ": ")))
                except ValueError:
                    print("Digite um número entre 1 e", p2.get_nattack())
                if a2 is None:
                    print("Digite um número entre 1 e", p2.get_nattack())
        else:
            print("%(name)s has no moves left!" % {"name": p2.get_name()})
            a2 = p2.select_attack(0)
        
        print()
        d.priority_duel(a1, a2)
    print("\nBatalha encerrada!")

def uso(name):
    """Imprime instruções de uso do programa."""
    print("""Este programa implementa uma batalha simples entre dois pokemons.

    -h --help       Imprime isto
    -b --billpc     Modifica a localização do diretório que contém os pokemons

Exemplos:

a) Entrada padrão do teclado:
%(name)s

b) Rodando o programa com os pokemons contidos em ./billpc/:
%(name)s -b

c) Selecionando a pasta que contém os pokemons:
%(name)s --billpc=<diretório>
""" % {"name": name})

def load(argv, str):
    print(str)
    p1 = p2 = None
    billpc = "billpc/"
    try:
        opt, args = getopt.getopt(argv, "hb", ["help", "billpc="])
    except (getopt.GetoptError):
        print("Opção inválida")
        uso(sys.argv[0])
        sys.exit(2)

    if len(argv) > 0:
        for o, a in opt:
            if o in ("-h", "--help"):
                uso(sys.argv[0])
                sys.exit()
            elif o == "-b":
                (p1, p2) = load_billpc(billpc)
            elif o == "--billpc":
                billpc = a
                (p1, p2) = load_billpc(billpc)
            else:
                assert False, "Opção inválida"
                sys.exit(2)
    else:
        (p1, p2) = load_keyboard()

    return (p1, p2)

def load2(argv, n, str):
    print(str)
    p1 = None
    billpc = "billpc/"
    try:
        opt, args = getopt.getopt(argv, "hb", ["help", "billpc="])
    except (getopt.GetoptError):
        print("Opção inválida")
        uso(sys.argv[0])
        sys.exit(2)

    if len(argv) > 0:
        for o, a in opt:
            if o in ("-h", "--help"):
                uso(sys.argv[0])
                sys.exit()
            elif o == "-b":
                p1 = load_billpc(billpc)
            elif o == "--billpc":
                billpc = a
                p1 = load_billpc(billpc)
            else:
                assert False, "Opção inválida"
                sys.exit(2)

    return p1

def main(argv):
    p1 = p2 = None
    (p1, p2) = load(argv, "Bem vindo a maior batalha pokemon de todos os tempos!")

    if p1 is not None and p2 is not None:
        battle(p1, p2)

if __name__ == '__main__':
    main(sys.argv[1:])
