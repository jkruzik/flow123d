#!/usr/bin/python
# -*- coding: utf-8 -*-
# author:   Jan Hybs
from ist.base import InputType
from ist.utils.texlist2 import TexList
from utils.logger import Logger


class LatexRecordDefault(object):
    @staticmethod
    def raw_format(tex, default):
        """
        :type tex: TexList
        :type default: ist.extras.TypeRecordKeyDefault
        """
        tex.add(default.value, tex.TYPE_NAME)

    @staticmethod
    def textlangle_format(tex, default):
        """
        :type tex: TexList
        :type default: ist.extras.TypeRecordKeyDefault
        """
        tex.macro_text_lr_angle(str(default.value).capitalize())

    @staticmethod
    def format(tex, default):
        """
        :type tex: TexList
        :type default: ist.extras.TypeRecordKeyDefault
        """
        LatexRecordDefault.format_rules.get(default.type).__func__(tex, default)

    format_rules = {
        'value at read time': raw_format,
        'value at declaration': textlangle_format,
        'optional': textlangle_format,
        'obligatory': textlangle_format,
        'default': textlangle_format
    }


class LatexRecord(TexList):
    def format(self, record):
        """
        % begin{RecordType}
        %       {<record name>}                 % name of the record, used for header and for hypertarget in form IT::<record name>
        %       {<parent abstract record>}      % possible parent abstract record
        %       {<default conversion key>}      % possible auto conversion key
        %       {<link>}                        % possible hyperlink into hand written text
        %       {< record description>}         % description of the record
        %
        %       \KeyItem{<name>}                % name of the key
        %               {<type>}                % type of the key
        %               {<default value>}       % type of default value and possibly the value itself
        %               {<link>}                % possible hyperlink to hand written text
        %               {<key description>}     % description of the key
        %       ...
        % end{RecordType}

        :type record: ist.nodes.TypeRecord
        """
        self.begin('RecordType')
        self._newline()
        self._tab()

        # name
        with self:
            self.macro_hyper_b(record)
        self._newline()
        self._tab()
        # implements
        with self:
            for impl in (record.implements or []):
                self.macro_alink(impl.get_reference())
        self._newline()
        self._tab()
        # conversion key
        with self:
            if record.reducible_to_key:
                self.macro_alink(record.reducible_to_key)
        self._newline()
        self._tab()
        # hyperlink into hand written text TODO
        # LATER it can removed since is not used anymore
        with self:
            pass
        self._newline()
        self._tab()
        # description
        with self:
            self.append(self.description(record.description))
        self._newline()
        # keys
        for key in (record.keys or []):
            self._newline()
            self._tab(2)
            with self.item_open('KeyItem'):
                self.macro_key(key)

        self.end('RecordType')

    def macro_key(self, record_key):
        """
        :type record_key: ist.extras.TypeRecordKey
        """
        # name
        self._newline()
        self._tab(3)
        with self:
            self.macro_hyper_b(record_key)
        # type
        self._newline()
        self._tab(3)
        with self:
            ref = record_key.type.get_reference()
            if ref.input_type == InputType.MAIN_TYPE:
                self.add(str(ref.input_type).capitalize())
                self.add(': ')
                self.macro_alink(record_key.type.get_reference())
            else:
                self.add(str(ref.input_type).capitalize())
        # default
        self._newline()
        self._tab(3)
        with self:
            LatexRecordDefault.format(self, record_key.default)
        # hyperlink into hand written text TODO
        # LATER it can removed since is not used anymore
        self._newline()
        self._tab(3)
        with self:
            pass
        # description
        self._newline()
        self._tab(3)
        with self:
            d = self.description(record_key.description)
            self.append(d)


class LatexSelection(TexList):
    def format(self, selection):
        """
        % begin{SelectionType}
        %       {<selection name>}
        %       {< selection description>}
        %
        %       \KeyItem
        %           {<value name>}
        %           {Key value description.}
        % end{SelectionType}

        :type selection: ist.nodes.TypeSelection
        """
        self.begin('SelectionType')

        # name
        with self:
            self.macro_hyper_b(selection)
        # description
        with self:
            self.append(self.description(selection.description))

        # values
        for key in (selection.values or []):
            with self.item_open('KeyItem'):
                self.macro_value(key)

        self.end('SelectionType')

    def macro_value(self, selection_value):
        """
        :type record_key: ist.extras.TypeSelectionValue
        """
        # name
        with self:
            self.macro_hyper_b(selection_value)
        # description
        with self:
            self.append(self.description(selection_value.description))


class LatexAbstractRecord(TexList):
    def format(self, abstract_record):
        """
        % begin{AbstractType}
        %       {<record name>}
        %       {<default descendant>}
        %       {<link>}
        %       {<description>}
        %       \Descendant{<type name>}
        % end{AbstractType}
        :type abstract_record: ist.nodes.TypeAbstract
        """
        self.begin('AbstractType')

        # name
        with self:
            self.macro_hyper_b(abstract_record)
        # descendant
        with self:
            if abstract_record.default_descendant:
                self.macro_alink(abstract_record.default_descendant.get_reference())
        with self:
                self.macro_add_doc(abstract_record)
        # description
        with self:
            self.append(self.description(abstract_record.description))

        for impl in abstract_record.implementations:
            with self.item_open('Descendant'):
                with self:
                    self.macro_alink(impl.get_reference())

        self.end('AbstractType')

class LatexFormatter(object):
    formatters = {
        'TypeRecord': LatexRecord,
        # 'TypeRecordKey': LatexRecordKey,
        'TypeAbstract': LatexAbstractRecord,
        'TypeAbstractRecord': LatexAbstractRecord,
        # 'TypeString': LatexString,
        'TypeSelection': LatexSelection,
        # 'TypeArray': LatexArray,
        # 'TypeInteger': LatexInteger,
        # 'TypeDouble': LatexDouble,
        # 'TypeBool': LatexBool,
        # 'TypeFilename': LatexFileName
    }

    @staticmethod
    def format(items):
        tex = TexList()

        Logger.instance().info('Processing items...')
        for item in items:
            # do no format certain objects
            if not item.include_in_format():
                Logger.instance().info(' - item skipped: %s' % str(item))
                continue

            Logger.instance().info(' - formatting item: %s' % str(item))
            # l = LatexRecord()
            # l.format(item)
            # print l
            # exit()
            fmt = LatexFormatter.get_formatter_for(item)
            if fmt is not None:
                fmt.format(item)
                tex.extend(fmt)

        return tex

    @staticmethod
    def get_formatter_for(o):
        cls = LatexFormatter.formatters.get(o.__class__.__name__, None)
        if cls is None:
            return None
        return cls()