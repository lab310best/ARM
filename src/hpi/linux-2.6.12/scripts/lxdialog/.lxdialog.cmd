cmd_scripts/lxdialog/lxdialog := gcc  -o scripts/lxdialog/lxdialog scripts/lxdialog/checklist.o scripts/lxdialog/menubox.o scripts/lxdialog/textbox.o scripts/lxdialog/yesno.o scripts/lxdialog/inputbox.o scripts/lxdialog/util.o scripts/lxdialog/lxdialog.o scripts/lxdialog/msgbox.o -lncurses 