

import clutter

def on_key_press_event(actor, event):
    if event.keyval == clutter.keysyms.Escape:
        clutter.main_quit()

stage = clutter.Stage()
stage.set_size(1280,800)
stage.connect('destroy', clutter.main_quit)
stage.connect('key-press-event', on_key_press_event)

timeline = clutter.Timeline(duration=1000)
labelalpha = clutter.Alpha(timeline,clutter.EASE_IN_OUT_SINE)
show_behaviour = clutter.BehaviourOpacity(0x00,255,alpha=labelalpha)

y = 0
for i in range(4096):
    label = clutter.Text()
    label.set_font_name("Mono 8")
    text = ''
    for j in range(8):
        text += 'C-5 80 .. .. %04x  ' % i
    text +='\n'
    label.set_text(text)
    label.set_opacity(0)
    label.set_position(0,y)
    y += label.get_height()
    stage.add(label)
    show_behaviour.apply(label)

stage.show_all()

timeline.start()

clutter.main()
