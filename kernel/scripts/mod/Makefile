hostprogs-y	:= modpost mk_elfconfig
hostprogs-$(CONFIG_LKM_ELF_HASH) += ksymhash
always		:= $(hostprogs-y) empty.o

modpost-objs	:= modpost.o file2alias.o sumversion.o elflib.o
ksymhash-objs	:= ksymhash.o elflib.o

ifdef CONFIG_LKM_ELF_HASH
modpost-objs += ktablehash.o
HOSTCFLAGS_ktablehash.o += -DCONFIG_LKM_ELF_HASH
HOSTCFLAGS_modpost.o += -DCONFIG_LKM_ELF_HASH
endif

# dependencies on generated files need to be listed explicitly

$(obj)/modpost.o $(obj)/file2alias.o $(obj)/sumversion.o: $(obj)/elfconfig.h
$(obj)/ksymhash.o $(obj)/ktablehash.o $(obj)/elflib.o: $(obj)/elfconfig.h

quiet_cmd_elfconfig = MKELF   $@
      cmd_elfconfig = $(obj)/mk_elfconfig $(ARCH) < $< > $@

$(obj)/elfconfig.h: $(obj)/empty.o $(obj)/mk_elfconfig FORCE
	$(call if_changed,elfconfig)

targets += elfconfig.h
