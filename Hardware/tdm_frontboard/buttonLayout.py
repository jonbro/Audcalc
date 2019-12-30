for x in range(0,5):
  for y in range(0,5):
    print("SW"+str(x+y*5+1));
    modref = "SW" + str(x+y*5+1);
    xmils = x*100;
    ymils = y*100;
    mod = board.FindModuleByReference(modref);
    mod.SetPosition(pcbnew.wxPointMils(xmils, ymils));
pcbnew.Refresh();
