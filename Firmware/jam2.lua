for i =0,7 do
setParam(i, 20, 0)
setParam(i, 21, 200)
setParam(i, 44, 0)
setParam(i, 45, 0)
setParam(i, 12, 175)
setParam(i, 47, 200)
end

print(collectgarbage("count"))

chords = {
    {0,5,9+12},
    {5,10,14+12},
    {3,8,12,15},
    {3,7,14+12},
    {0,5,0,9+12,5},
    {5,10,14+12,10-12},
    {3,8,12,15+12},
    {3,7,14},
}
cc = 1

function getBleeper(i, c)
    local bl = {}
    bl.i = i
    bl.countdown = c
    bl.reset = c
    bl.s = 1
    bl.vol = 60
    bl.volDir = 1
    bl.octave = 0
    function bl:trigger()
        self.countdown = self.countdown - 1
        if(self.countdown == 0) then
            self.countdown = self.reset
            if(self.s > #chords[cc]) then
                self.s = 1
            end
            local tc = chords[cc][self.s]+60+self.octave*12
            playNote(self.i, tc)
            bl.vol = bl.vol+self.volDir
            if(self.vol > 80) then self.volDir = -1 end
            if(self.vol < 30) then self.volDir = 1 end
            setParam(i, 14, self.vol)
            setParam(i, 11, self.vol+100)
            setParam(i, 10, self.vol+30)
            self.s = self.s+1
            --if math.random() > 0.97 then self.countdown = self.countdown*2 end
        end
    end
    return bl
end

cBl = getBleeper(0,23*24)
function cBl:trigger()
    self.countdown = self.countdown - 1
    if(self.countdown == 0) then
        self.countdown = self.reset
        cc = cc+1
        if cc > #chords then
            cc = 1
        end
    end
end
mel = getBleeper(7,23*13)
setParam(7, 12, 180)
setParam(7, 20, 120)
setParam(7, 21, 120)
setParam(7, 44, 100)
setParam(7, 45, 100)

function mel:trigger()
    self.countdown = self.countdown - 1
    if(self.countdown == 0) then
        self.countdown = self.reset
        self.s = self.s+math.floor(math.random(1, 4))
        while(self.s > #chords[cc]) do
            self.s = self.s - #chords[cc]
        end
        local tc = chords[cc][self.s]+60
        if(math.random() > 0.7) then tc = tc +5 end
        playNote(self.i, tc)
    end
end

local voices = {}
local primes = {7,11,13,17,19,23,29,31,37}
for i=0,3 do
    local v = getBleeper(i,primes[(i%2)+4]);
    table.insert(voices,v)
    if(i%2==0) then v.s = v.s+1 end
end
voices[2].octave=2
table.insert(voices, cBl)
table.insert(voices, mel)
local s = 2
function tempoSync()
    s = s - 1
    if s == 0 then
        s = 2
        for k,v in ipairs(voices) do
            v:trigger()
        end
    end
end