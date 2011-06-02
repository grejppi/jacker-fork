
env = Environment()

jacker = env.SConscript(dirs=['src'], 
    variant_dir='#build/$variant_dir', duplicate=0,
    exports = ['env'])

env.Install("$install_bin_dir", jacker)

share_dir = "$install_share_dir/jacker"
env.Install(share_dir, "res/jacker.glade")
env.Install(share_dir, "res/jacker.png")
env.Install(share_dir, "cheatsheet.txt")
env.Install(share_dir, "commands.txt")
