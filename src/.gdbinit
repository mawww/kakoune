set print pretty
break Kakoune::on_assert_failed

python
sys.path.insert(0, '../gdb/')
import gdb.printing
import kakoune
gdb.printing.register_pretty_printer(
        gdb.current_objfile(),
        kakoune.build_pretty_printer())
end
