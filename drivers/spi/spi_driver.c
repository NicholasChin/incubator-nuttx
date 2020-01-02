/****************************************************************************
 * drivers/spi/spi_driver.c
 *
 *   Copyright (C) 2016-2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <nuttx/spi/spi_transfer.h>

#ifdef CONFIG_SPI_DRIVER

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Device naming ************************************************************/

#define DEVNAME_FMT    "/dev/spi%d"
#define DEVNAME_FMTLEN (8 + 3 + 1)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Driver state structure */

struct spi_driver_s
{
  FAR struct spi_dev_s *spi;  /* Contained SPI lower half driver */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  sem_t exclsem;              /* Mutual exclusion */
  int16_t crefs;              /* Number of open references */
  bool unlinked;              /* True, driver has been unlinked */
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int     spidrvr_open(FAR struct file *filep);
static int     spidrvr_close(FAR struct file *filep);
#endif
static ssize_t spidrvr_read(FAR struct file *filep, FAR char *buffer,
                 size_t buflen);
static ssize_t spidrvr_write(FAR struct file *filep, FAR const char *buffer,
                 size_t buflen);
static int     spidrvr_ioctl(FAR struct file *filep, int cmd,
                 unsigned long arg);
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int     spidrvr_unlink(FAR struct inode *inode);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations spidrvr_fops =
{
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  spidrvr_open,    /* open */
  spidrvr_close,   /* close */
#else
  NULL,            /* open */
  NULL,            /* close */
#endif
  spidrvr_read,    /* read */
  spidrvr_write,   /* write */
  NULL,            /* seek */
  spidrvr_ioctl,   /* ioctl */
  NULL             /* poll */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  , spidrvr_unlink /* unlink */
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: spidrvr_open
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int spidrvr_open(FAR struct file *filep)
{
  FAR struct inode *inode;
  FAR struct spi_driver_s *priv;
  int ret;

  /* Get our private data structure */

  DEBUGASSERT(filep != NULL && filep->f_inode != NULL);
  inode = filep->f_inode;

  priv = (FAR struct spi_driver_s *)inode->i_private;
  DEBUGASSERT(priv);

  /* Get exclusive access to the SPI driver state structure */

  ret = nxsem_wait(&priv->exclsem);
  if (ret < 0)
    {
      return ret;
    }

  /* Increment the count of open references on the driver */

  priv->crefs++;
  DEBUGASSERT(priv->crefs > 0);

  nxsem_post(&priv->exclsem);
  return OK;
}
#endif

/****************************************************************************
 * Name: spidrvr_close
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int spidrvr_close(FAR struct file *filep)
{
  FAR struct inode *inode;
  FAR struct spi_driver_s *priv;
  int ret;

  /* Get our private data structure */

  DEBUGASSERT(filep != NULL && filep->f_inode != NULL);
  inode = filep->f_inode;

  priv = (FAR struct spi_driver_s *)inode->i_private;
  DEBUGASSERT(priv);

  /* Get exclusive access to the SPI driver state structure */

  ret = nxsem_wait(&priv->exclsem);
  if (ret < 0)
    {
      return ret;
    }

  /* Decrement the count of open references on the driver */

  DEBUGASSERT(priv->crefs > 0);
  priv->crefs--;

  /* If the count has decremented to zero and the driver has been unlinked,
   * then commit Hara-Kiri now.
   */

  if (priv->crefs <= 0 && priv->unlinked)
    {
      nxsem_destroy(&priv->exclsem);
      kmm_free(priv);
      return OK;
    }

  nxsem_post(&priv->exclsem);
  return OK;
}
#endif

/****************************************************************************
 * Name: spidrvr_read
 ****************************************************************************/

static ssize_t spidrvr_read(FAR struct file *filep, FAR char *buffer,
                            size_t len)
{
  return 0; /* Return EOF */
}

/****************************************************************************
 * Name: spidrvr_write
 ****************************************************************************/

static ssize_t spidrvr_write(FAR struct file *filep, FAR const char *buffer,
                             size_t len)
{
  return len; /* Say that everything was written */
}

/****************************************************************************
 * Name: spidrvr_ioctl
 ****************************************************************************/

static int spidrvr_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  FAR struct inode *inode;
  FAR struct spi_driver_s *priv;
  FAR struct spi_sequence_s *seq;
  int ret;

  spiinfo("cmd=%d arg=%lu\n", cmd, arg);

  /* Get our private data structure */

  DEBUGASSERT(filep != NULL && filep->f_inode != NULL);
  inode = filep->f_inode;

  priv = (FAR struct spi_driver_s *)inode->i_private;
  DEBUGASSERT(priv);

  /* Get exclusive access to the SPI driver state structure */

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  ret = nxsem_wait(&priv->exclsem);
  if (ret < 0)
    {
      return ret;
    }
#endif

  /* Process the IOCTL command */

  switch (cmd)
    {
      /* Command:      SPIIOC_TRANSFER
       * Description:  Perform a sequence of SPI transfers
       * Argument:     A reference to an instance of struct spi_sequence_s.
       * Dependencies: CONFIG_SPI_DRIVER
       */

      case SPIIOC_TRANSFER:
        {
          /* Get the reference to the spi_transfer_s structure */

          seq = (FAR struct spi_sequence_s *)((uintptr_t)arg);
          DEBUGASSERT(seq != NULL);

          /* Perform the transfer */

          ret = spi_transfer(priv->spi, seq);
        }
        break;

      default:
        ret = -ENOTTY;
        break;
    }

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  nxsem_post(&priv->exclsem);
#endif
  return ret;
}

/****************************************************************************
 * Name: spidrvr_unlink
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int spidrvr_unlink(FAR struct inode *inode)
{
  FAR struct spi_driver_s *priv;
  int ret;

  /* Get our private data structure */

  DEBUGASSERT(inode != NULL && inode->i_private != NULL);
  priv = (FAR struct spi_driver_s *)inode->i_private;

  /* Get exclusive access to the SPI driver state structure */

  ret = nxsem_wait(&priv->exclsem);
  if (ret < 0)
    {
      return ret;
    }

  /* Are there open references to the driver data structure? */

  if (priv->crefs <= 0)
    {
      nxsem_destroy(&priv->exclsem);
      kmm_free(priv);
      return OK;
    }

  /* No... just mark the driver as unlinked and free the resouces when the
   * last client closes their reference to the driver.
   */

  priv->unlinked = true;
  nxsem_post(&priv->exclsem);
  return ret;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: spi_register
 *
 * Description:
 *   Create and register the SPI character driver.
 *
 *   The SPI character driver is a simple character driver that supports SPI
 *   transfers.  The intent of this driver is to support SPI testing.  It is
 *   not suitable for use in any real driver application.
 *
 * Input Parameters:
 *   spi - An instance of the lower half SPI driver
 *   bus - The SPI bus number.  This will be used as the SPI device minor
 *     number.  The SPI character device will be registered as /dev/spiN
 *     where N is the minor number
 *
 * Returned Value:
 *   OK if the driver was successfully register; A negated errno value is
 *   returned on any failure.
 *
 ****************************************************************************/

int spi_register(FAR struct spi_dev_s *spi, int bus)
{
  FAR struct spi_driver_s *priv;
  char devname[DEVNAME_FMTLEN];
  int ret;

  /* Sanity check */

  DEBUGASSERT(spi != NULL && (unsigned)bus < 1000);

  /* Allocate a SPI character device structure */

  priv = (FAR struct spi_driver_s *)kmm_zalloc(sizeof(struct spi_driver_s));
  if (priv)
    {
      /* Initialize the SPI character device structure */

      priv->spi = spi;
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
      nxsem_init(&priv->exclsem, 0, 1);
#endif

      /* Create the character device name */

      snprintf(devname, DEVNAME_FMTLEN, DEVNAME_FMT, bus);
      ret = register_driver(devname, &spidrvr_fops, 0666, priv);
      if (ret < 0)
        {
          /* Free the device structure if we failed to create the character
           * device.
           */

          kmm_free(priv);
          return ret;
        }

      /* Return the result of the registration */

      return ret;
    }

  return -ENOMEM;
}

#endif /* CONFIG_SPI_DRIVER */
