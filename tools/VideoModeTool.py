##############################################################################/
##
## ZX-ESPectrum - ZX Spectrum emulator for ESP32
## Modeline Calculator tool
## 
## Copyright (c) 2023 David Crespo [dcrespo3d]
## https://github.com/dcrespo3d
##
## APLL parameters calculation adapted from Jean Thomas' calculator
## https://github.com/jeanthom/ESP32-APLL-cal
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to
## deal in the Software without restriction, including without limitation the
## rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
## sell copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
## 
## The above copyright notice and this permission notice shall be included in
## all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
## DEALINGS IN THE SOFTWARE.
##

# tk reference: https://anzeljg.github.io/rin2/book2/2405/docs/tkinter/index.html
from tkinter import *

window = Tk()
window.title('ESPectrum Video Mode Tool - by David Crespo [github.com/dcrespo3d]')

# fr for frame
# la for label
# en for entry
# de for disabled entry

# hactive, hfront, hsync, hback

class APLLCalc:
    def __init__(self):
        # ESP32 Xtal frequency
        self.fXtal = 40000000.0

    def calc(self, fTarget):
        fTarget = float(fTarget)
        temp = 0.0

        # REV0
        bestR0Delta = 1.0e30
        self.bestR0sdm2 = 0
        self.bestR0odiv = 0

        for sdm2 in range(64):
            # Checking for frequency range of the numerator
            temp = self.fXtal * (sdm2 + 4)
            if temp > 500000000.0 or temp < 350000000.0:
                continue

            for odiv in range(32):
                fOut = temp / float(2*(odiv+2))
                if abs(fOut-fTarget) < bestR0Delta:
                    bestR0Delta = abs(fOut-fTarget)
                    self.bestR0sdm2 = sdm2
                    self.bestR0odiv = odiv
        
        self.fOutR0 = self.fXtal * (self.bestR0sdm2 + 4) / float(2*(self.bestR0odiv+2))
        self.fDeltaR0 = abs(self.fOutR0-fTarget)
        self.fErrPctR0 = 100*abs(self.fOutR0-fTarget)/fTarget

        # REV1
        bestR1Delta = 1.0e30
        self.bestR1sdm0 = 0
        self.bestR1sdm1 = 0
        self.bestR1sdm2 = 0
        self.bestR1odiv = 0

        for sdm0 in range(256):
            for sdm1 in range(256):
                for sdm2 in range(64):
                    # Checking for frequency range of the numerator
                    temp = self.fXtal * (sdm2 + float(sdm1/256.0) + float(sdm0/65536.0) + 4)
                    if temp > 500000000.0 or temp < 350000000.0:
                        continue

                    for odiv in range(32):
                        fOut = temp / float(2*(odiv+2))
                        if abs(fOut-fTarget) < bestR1Delta:
                            bestR1Delta = abs(fOut-fTarget)
                            self.bestR1sdm0 = sdm0
                            self.bestR1sdm1 = sdm1
                            self.bestR1sdm2 = sdm2
                            self.bestR1odiv = odiv

        self.fOutR1 = self.fXtal * (self.bestR1sdm2 + self.bestR1sdm1/256.0 + self.bestR1sdm0/65536.0 + 4) / float(2*(self.bestR1odiv+2))
        self.fDeltaR1 = abs(self.fOutR1-fTarget)
        self.fErrPctR1 = 100*abs(self.fOutR1-fTarget)/fTarget

    def calcAndPrint(self, fTarget):
        self.calc(fTarget)
        print("================================================================================");
        print("Revision 0")
        print("f=%.01f Hz, ∆f=%.01f Hz (relative: %.04f%%)"%(self.fOutR0, self.fDeltaR0, self.fErrPctR0));
        print("sdm2=%d, odiv=%d\n"%(self.bestR0sdm2, self.bestR0odiv));
        print("================================================================================");
        print("Revision 1")
        print("f=%.01f Hz, ∆f=%.01f Hz (relative: %.04f%%)"%(self.fOutR1, self.fDeltaR1, self.fErrPctR1));
        print("sdm0=%d, sdm1=%d, sdm2=%d, odiv=%d\n"%(self.bestR1sdm0, self.bestR1sdm1, self.bestR1sdm2, self.bestR1odiv));
        print("================================================================================");
        

class ParamCtl:
    def __init__(self, parent, text, width):
        self.parent = parent
        self.text = text
        self.fr = Frame(self.parent)
        self.la = Label(self.fr, text=self.text)
        self.la.pack(fill=X, side=TOP, padx=5, pady=2)
        self.en = Entry(self.fr, width=width, justify=CENTER)
        self.en.pack(fill=X, side=TOP, padx=5, pady=2)
        self.fr.pack(side=LEFT, padx=5, pady=2)

    def set(self, enval):
        try:
            if int(enval) == float(enval): enval = int(enval)
        except: pass
        self.en.delete(0, END)
        self.en.insert(END, str(enval))

class ParamDuo:
    def __init__(self, parent, text, subtext=None, width=8):
        self.parent = parent
        self.text = text
        self.fr = Frame(self.parent)
        self.la = Label(self.fr, text=self.text)
        self.la.pack(fill=X, side=TOP, padx=5, pady=2)
        self.en = Entry(self.fr, width=width, justify=CENTER)
        self.en.pack(fill=X, side=TOP, padx=5, pady=2)
        self.de = Entry(self.fr, width=width, justify=CENTER, state=DISABLED)
        self.de.pack(fill=X, side=TOP, padx=5, pady=2)
        if subtext: self.set(deval=subtext)
        self.fr.pack(side=LEFT, padx=5, pady=2)

    def set(self, enval=None, deval=None):
        if enval:
            try:
                if int(enval) == float(enval): enval = int(enval)
            except: pass
            self.en.delete(0, END)
            self.en.insert(END, str(enval))
        if deval:
            try:
                if int(deval) == float(deval): deval = int(deval)
            except: pass
            self.de.configure(state=NORMAL)
            self.de.delete(0, END)
            self.de.insert(END, str(deval))
            self.de.configure(state=DISABLED)

class HVParamFrame:
    def __init__(self, parent, pfx):
        self.parent = parent
        self.pfx = pfx
        self.fr = Frame(parent)
        self.act = ParamDuo(self.fr, pfx + 'Active')
        self.fro = ParamDuo(self.fr, pfx + 'Front')
        self.syn = ParamDuo(self.fr, pfx + 'Sync')
        self.bak = ParamDuo(self.fr, pfx + 'Back')
        self.fre = ParamDuo(self.fr, pfx + 'Freq', 'Hz', 12)
        self.fr.pack(padx=5, pady=5)

    def set(self, act, fro, syn, bak):
        self.act.set(act)
        self.fro.set(fro)
        self.syn.set(syn)
        self.bak.set(bak)

    def setFreq(self, fre):
        self.fre.set(fre)


    def calcsum(self):
        act = float(self.act.en.get())
        fro = float(self.fro.en.get())
        syn = float(self.syn.en.get())
        bak = float(self.bak.en.get())
        self.act.set(deval=act)
        self.fro.set(deval=act+fro)
        self.syn.set(deval=act+fro+syn)
        self.bak.set(deval=act+fro+syn+bak)
        return act+fro+syn+bak

class APLLParamFrame:
    def __init__(self, parent=None, rev='-'):
        self.parent = parent
        self.fr = Frame(parent)
        self.sdm0 = ParamDuo(self.fr, 'sdm0', rev)
        self.sdm1 = ParamDuo(self.fr, 'sdm1', rev)
        self.sdm2 = ParamDuo(self.fr, 'sdm2', rev)
        self.odiv = ParamDuo(self.fr, 'odiv', rev)
        self.fout = ParamDuo(self.fr, 'pixclk[Hz]', rev)
        self.fdel = ParamDuo(self.fr, '∆f [Hz]', rev)
        self.ferr = ParamDuo(self.fr, 'err (%)', rev)
        self.fr.pack(padx=5, pady=5)

# pixclk = htot * vtot * vfreq

def calculate():
    global pixclk
    htot = hprms.calcsum()
    vtot = vprms.calcsum()
    # if scandouble.get():
    #     vtot *= 2

    if calcmode.get() == 0:        
        pixclk = float(pixprms.en.get())
        vfreq = pixclk / (htot * vtot)
        hfreq = pixclk / htot
        print(htot, vtot, pixclk, vfreq, hfreq)
        vprms.setFreq(vfreq)
        hprms.setFreq(hfreq)
    if calcmode.get() == 1:
        vfreq = float(vprms.fre.en.get())
        hfreq = vfreq * vtot
        pixclk = hfreq * htot
        hprms.setFreq(hfreq)
        pixprms.set(pixclk)
    if calcmode.get() == 2:
        hfreq = float(hprms.fre.en.get())
        vfreq = hfreq / vtot
        pixclk = hfreq * htot
        vprms.setFreq(vfreq)

def calculateAPLL():
    apllCalc.calcAndPrint(pixclk)

    apllfr0.sdm0.set('-')
    apllfr0.sdm1.set('-')
    apllfr0.sdm2.set(str(int(apllCalc.bestR0sdm2)))
    apllfr0.odiv.set(str(int(apllCalc.bestR0odiv)))
    apllfr0.fout.set(str(int(apllCalc.fOutR0)))
    apllfr0.fdel.set(str(int(apllCalc.fDeltaR0)))
    apllfr0.ferr.set(str(int(apllCalc.fErrPctR0)))

    apllfr1.sdm0.set(str(int(apllCalc.bestR1sdm0)))
    apllfr1.sdm1.set(str(int(apllCalc.bestR1sdm1)))
    apllfr1.sdm2.set(str(int(apllCalc.bestR1sdm2)))
    apllfr1.odiv.set(str(int(apllCalc.bestR1odiv)))
    apllfr1.fout.set(str(int(apllCalc.fOutR1)))
    apllfr1.fdel.set(str(int(apllCalc.fDeltaR1)))
    apllfr1.ferr.set(str(int(apllCalc.fErrPctR1)))

def genModeline():
    s = 'const Mode VGA::MODE'
    hact = int(hprms.act.en.get())
    hfro = int(hprms.fro.en.get())
    hsyn = int(hprms.syn.en.get())
    hbak = int(hprms.bak.en.get())

    vact = int(vprms.act.en.get())
    vfro = int(vprms.fro.en.get())
    vsyn = int(vprms.syn.en.get())
    vbak = int(vprms.bak.en.get())

    if scandouble.get():
        s += str(hact) + 'x' + str(int(vact/2))
    else:
        s += str(hact) + 'x' + str(vact)
    s += '('
    s += str(hfro) + ', '
    s += str(hsyn) + ', '
    s += str(hbak) + ', '
    s += str(hact) + ', '
    s += str(vfro) + ', '
    s += str(vsyn) + ', '
    s += str(vbak) + ', '
    s += str(vact) + ', '

    if scandouble.get():
        s += '2' + ', '
    else:
        s += '1' + ', '

    s += str(int(pixclk)) + ', '

    s += str(hsyncneg.get()) + ', '
    s += str(vsyncneg.get()) + ', '

    try:
        s += str(apllCalc.bestR1sdm0) + ','
        s += str(apllCalc.bestR1sdm1) + ','
        s += str(apllCalc.bestR1sdm2) + ','
        s += str(apllCalc.bestR1odiv) + ','
        s += str(0) + ','
        s += str(0) + ','
        s += str(apllCalc.bestR0sdm2) + ','
        s += str(apllCalc.bestR0odiv) + ');'
    except:
        s += '0, 0, 0, 0, 0, 0, 0, 0);'

    modeline.set(s)

def parseModeline():
    pass

hprms = HVParamFrame(None, 'h')
vprms = HVParamFrame(None, 'v')

pixclk = 7000000

cntfr = Frame()

pixfr = Frame(cntfr)
pixprms = ParamDuo(pixfr, 'Pixel Clock', 'Hz', 10)
pixprms.set(pixclk)
pixfr.pack(padx=5, pady=5, side=LEFT)

calcfr = Frame(cntfr)
btncalc = Button(calcfr, text='Calculate', command=calculate)
btncalc.pack(padx=20,pady=1)

btncalc2 = Button(calcfr, text='Calc APLL', command=calculateAPLL)
btncalc2.pack(padx=20,pady=1)

btngenml = Button(calcfr, text='Generate modeline', command=genModeline)
btngenml.pack(padx=20,pady=1)

btnparse = Button(calcfr, text='Parse modeline', command=parseModeline, state=DISABLED)
btnparse.pack(padx=20,pady=1)

calcfr.pack(padx=5, pady=5, side=LEFT)

rbfr = Frame(cntfr)
calcmode = IntVar()
rb0 = Radiobutton(rbfr, text="given pixclk, calc others", variable=calcmode, value=0).pack(side=TOP, anchor="w")
rb1 = Radiobutton(rbfr, text="given vFreq, calc others", variable=calcmode, value=1).pack(side=TOP, anchor="w")
rb2 = Radiobutton(rbfr, text="given hFreq, calc others", variable=calcmode, value=2).pack(side=TOP, anchor="w")
calcmode.set(0)
rbfr.pack(side=LEFT)

optfr = Frame(cntfr)
scandouble = IntVar()
scandouble.set(0)
hsyncneg = IntVar()
hsyncneg.set(0)
vsyncneg = IntVar()
vsyncneg.set(0)
Checkbutton(optfr, text='ScanDouble', variable=scandouble).pack()
Checkbutton(optfr, text='H Sync Negative', variable=hsyncneg).pack()
Checkbutton(optfr, text='V Sync Negative', variable=vsyncneg).pack()
optfr.pack(side=LEFT)

cntfr.pack()

apllfr = Frame()
apllfr0 = APLLParamFrame(apllfr, 'rev0')
apllfr1 = APLLParamFrame(apllfr, 'rev1')
apllfr.pack()

modeline = ParamCtl(None, 'Modeline for ESPectrum code', 90)

apllCalc = APLLCalc()

hprms.set(320, 38, 32, 58)
vprms.set(240, 28, 3, 41)

hprms.calcsum()
vprms.calcsum()



# exit on ESC keypress
def handle_keypress(event):
    #print(event.char)
    #print(event)
    if event.keycode == 27:
        exit()

# Bind keypress event to handle_keypress()
window.bind("<Key>", handle_keypress)

window.mainloop()